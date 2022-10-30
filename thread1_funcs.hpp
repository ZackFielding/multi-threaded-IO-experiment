#ifndef THREAD1_FUNCS_HPP
#define THREAD1_FUNCS_HPP

#include <iostream>
#include <thread>
#include <string>
#include <array>
#include <vector>
#include <fstream>
#include <memory>
#include <deque>
#include <cctype>
#include <cmath>
#include <mutex>
#include <condition_variable>

#define CONSOLE_LOG std::clog
#define CONSOLE_ERROR std::cerr
#define CONSOLE_OUT std::cout

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

size_t getNumberCount(std::ifstream& file)
{
	const auto file_start_pos {file.tellg()};
	size_t num_count {0};
	constexpr std::array<char, 3> delim_array {' ', '\n', '\r'};
	while (readMultipleDelim<delim_array.size()>(file, delim_array))
		++num_count;

	 // clear failbit flag && seek back to start
	file.clear();
	file.seekg(file_start_pos, std::ios_base::beg);	

	CONSOLE_LOG << "Vector size: " << num_count << '\n';
	return num_count;
}

std::mutex sleep_mut, pop_mut;
std::condition_variable g_con_var;

void reverseFindPos(const double max_read, std::ifstream& file, std::deque<double>& pos_que)
{
	 // start off at EOF - 1 posisition to read
	long long cur_pos {file.tellg()};
	cur_pos -= 1L;
	file.seekg(cur_pos, std::ios_base::beg);
	 // c_pair will hold x2 char + null term for reading
	char c_pair [] {"A"}; //null terminated - sizeof == 2
	 // keeps track of numbers that WILL BE read (based on array size) 
	unsigned long long cur_num_reads {0};

	// NOT ENTERING WHILE LOOP -> file may have fail flag set? .get() might be issue
	std::unique_lock<std::mutex> LOCK (sleep_mut);
	while (cur_num_reads < max_read)
	{
		if (file.peek() != 10)
		{
			// if not newline/carriage return -> extract (or else get fails)
			// if get == white space && peek (next char) != white space -> push into deque obj
			// if get != white space OR peek == white space -> move backwards in file read position
			if (std::isspace(file.get(c_pair, sizeof c_pair)) != 0  && std::issapce(file.peek()) == 0)
			{
				pos_que.push_back(static_cast<double>(cur_pos)); // cast to double as need to handle NAN value
				g_con_var.notify_one(); // notify writing thread
				++cur_num_reads; // increment to keep track of number of reads
			}
		}
		 // does not matter if found read pos or not -> set back two positions in read file
		cur_pos -= 2L;
		file.seekg(cur_pos, std::ios_base::beg);
	}
	
	 // once max number of reverse file reads occurs -> push NAN to deque to stop extraction thread
	pos_que.push_back(std::nan("NAN"));	
	file.close(); // close file [read_files.at(1) via main]
}

// remove max read and array size
bool reverseGet(std::unique_lock<std::mutex>& LOCK, std::deque<double>& pos_que, 
		std::vector<int>& written_vector)
{
		// returns true if incurs NAN -> false if it never does (error)
		// while condition + NAN both control for never accessing empty deque
	std::ifstream duplicate_RRF;
	duplicate_RRF.open("test-read.txt");
	double seekg_d {}, abs_stop_pos {written_vector.size()/2}, 
		   cur_vec_pos {written_vector.size()-1};
	std::stringstream char_to_double_stream; // middle-man between file read & double vector write
	std::array<char, 8> temp_char_hold_arr;

	while (cur_vec_pos > abs_stop_pos)
	{
		// pass in LOCK 
		if (pos_que.empty())
			g_con_var.wait(LOCK);

		 // read front of deque
		seekg_d = pos_que.front();
		// if read != NAN 
		if (!std::isnan(seekg_d))
		{
			 // seek duplicate reverse read file to read pos rel to file start
			duplicate_RRF.seekg(seekg_d, std::ios_base::beg);
			// write to vector, starting from last index position
			if (duplicate_RRF >> written_vector.at(cur_vec_pos++))
			{
				 // if read is successful
				   // lock deque -> remove element it just read -> unlock deque
				pop_mut.lock();
				pos_que.pop_front();
				pop_mut.unlock();
			}
		}
		else // if read == NAN - reverse read finished (no more writing required)
		{
			duplicate_RRF.close();
			return true; // true signals successful reverse file read
		}
	}
	return false;
}

void reverseFileRead(std::ifstream& file, std::vector<int>& written_vector)
{
	 // que that will be accessed by multiple threads
	std::deque<double> pos_que;
	std::thread reverse_read_thread(
			&reverseFindPos,
			written_vector.size()/2,
			std::ref(file),
			std::ref(pos_que));

	std::thread reverse_write_thread
		([](std::deque<double>& pos_que, std::vector<int>& written_vector)
		 {
		 	std::unique_lock<std::mutex> LOCK (sleep_mut); //aquire cond lock
			if (reverseGet(
						LOCK, // unique_lock by ref
						pos_que, // deque obj
						written_vector // vector to be read to
					 ))
				CONSOLE_LOG << "Reverse get func exited normally.\n"; 
			else 
				CONSOLE_LOG << "Reverse get func exited without incurring NAN.\n";
		 },
		std::ref(pos_que), std::ref(written_vector));

	reverse_read_thread.join();
	reverse_write_thread.join();
}

#endif
