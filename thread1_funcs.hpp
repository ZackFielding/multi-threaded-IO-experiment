#ifndef THREAD1_FUNCS_HPP
#define THREAD1_FUNCS_HPP

#include <iostream>
#include <thread>
#include <string>
#include <array>
#include <fstream>
#include <memory>
#include <deque>
#include <cctype>
#include <cmath>
#include <mutex>
#include <condition_variable>

template<const size_t AR_SIZE>
bool loop_delim(const char current_character, const std::array<char, AR_SIZE>& delims)
{
	for (size_t i {0}; i < AR_SIZE; ++i)
	{
		if (current_character == delims.at(i))
			return false;
	}
	return true;
}

template<const size_t AR_SIZE>
bool readMultipleDelim(std::ifstream& file, const std::array<char, AR_SIZE>& delims)
{
	static char hold;
	while (file.get(hold))
	{
		if (loop_delim<delims.size()>(hold, delims))
		{
			if (!loop_delim<delims.size()>(((char)file.peek()), delims))
				return true;
		}
	}
	return false;
}

unsigned long long getNumberCount(std::ifstream& file)
{
	const auto file_start_pos {file.tellg()};
	unsigned long long num_count {0};
	constexpr std::array<char, 3> delim_array {' ', '\n', '\r'};
	while (readMultipleDelim<delim_array.size()>(file, delim_array))
		++num_count;

	 // clear failbit flag && seek back to start
	file.clear();
	file.seekg(file_start_pos, std::ios_base::beg);	

	return num_count;
}

std::mutex sleep_mut, pop_mut;
std::condition_variable g_con_var;

void reverseFindPos(const unsigned long long max_read, std::ifstream& file, std::deque<double>& pos_que)
{
	 // start off at EOF - 2 posisition to read
	long long cur_pos {file.tellg()};
	cur_pos -= 2L;
	 // c_pair will hold x2 char + null term for reading
	char c_pair [] {"A"}; //null terminated - sizeof == 2
	 // keeps track of numbers that WILL BE read (based on array size) 
	unsigned long long cur_num_reads {0};

	std::unique_lock<std::mutex> LOCK (sleep_mut);
	while (cur_num_reads < max_read && file.get(c_pair, sizeof c_pair))
	{
			// if lead IS space character AND lead+1 is NOT a space character
			 // use peek for lead+1 to prevent EOF flag trigger
		if (std::isspace(c_pair[0]) != 0 && std::isspace(file.peek()) == 0)
		{
			 // push current index into que
			pos_que.push_back(static_cast<double>(cur_pos));
			g_con_var.notify_one(); // notify writing thread
			++cur_num_reads; // increment to keep track of number of reads
			cur_pos -= 3;
		}
		else
			cur_pos -= 2;
		 // set new read position for next loop
		file.seekg(cur_pos, std::ios_base::end);
	}
	
	 // when while fails -> push NAN to deque to stop extraction thread
	pos_que.push_back(std::nan("NAN"));	
}

bool reverseGet(std::unique_lock<std::mutex>& LOCK, unsigned long long arr_index, 
		unsigned long long max_read, std::deque<double>& pos_que, int array_heap[])
{
		// returns true if incurs NAN -> false if it never does (error)
		// while condition + NAN both control for never accessing empty deque
	std::ifstream duplicate_RRF;
	duplicate_RRF.open("test-read.txt", std::ios_base::ate);

	double seekg_d; // double as it needs to accept NAN
	unsigned long long stop_pos {(arr_index--) - (max_read)};
	while(arr_index >= stop_pos)
	{
		// pass in LOCK or create unique_lock here?
		if (pos_que.empty())
			g_con_var.wait(LOCK);

		 // read front of deque
		seekg_d = pos_que.front();
		// if read == NAN -> return true (error-free run) 
		if (!std::isnan(seekg_d))
		{
			duplicate_RRF.seekg(static_cast<long long>(seekg_d),
					std::ios_base::beg);
			// write to heap allocated array - starting from back
			if (duplicate_RRF >> array_heap[arr_index--])
			{
				 // lock deque -> remove element it just read -> unlock deque
				pop_mut.lock();
				pos_que.pop_front();
				pop_mut.unlock();
			}
		}
		else
			return true;
	}
	return false;
}

void reverseFileRead(const unsigned long long arr_size, const unsigned long long max_read,
		std::ifstream& file, int array_heap[])
{
	 // que that will be accessed by multiple threads
	std::deque<double> pos_que;
	std::thread reverse_read_thread(
			&reverseFindPos,
			max_read,
			std::ref(file),
			std::ref(pos_que));

	std::thread reverse_write_thread
		([=](std::deque<double>& pos_que, int* array_heap)
		 {
		 	std::unique_lock<std::mutex> LOCK (sleep_mut); //aquire cond lock
			if (reverseGet(
						LOCK, // unique_lock by ref
						arr_size, // array size
						max_read, // number of elements to index into array
						pos_que, // deque obj
						array_heap // array to be read to
					 ))
				std::clog << "Reverse get func exited normally.\n";
			else
				std::clog << "Reverse get func exited without incurring NAN.\n";
		 },
		std::ref(pos_que), array_heap);

	reverse_read_thread.join();
	reverse_write_thread.join();
}

#endif
