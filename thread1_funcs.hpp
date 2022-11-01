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
#include <sstream>

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

	//CONSOLE_LOG << "Vector size: " << num_count << '\n';
	return num_count; 
}

std::mutex sleep_mut, pop_mut;
std::condition_variable g_con_var;

void reverseFindPos(const double max_read, std::ifstream& file, std::deque<double>& pos_que)
{
	 // start off at EOF - 1 posisition to read
	long long cur_pos {file.tellg()};
	cur_pos -= 3L; // skip over new line and carriage return characters prior to EOF
	file.seekg(cur_pos, std::ios_base::beg);
	char c_pair[] {"A"}; // c_pair needs to be array for get() with delim (needs to be null terminated)
	unsigned long long cur_num_reads {0}; // keeps track of numbers that WILL BE read (based on array size) 
	constexpr char null_char {static_cast<char>(0)}; // needs to be a char NOT in reading document

	std::unique_lock<std::mutex> LOCK (sleep_mut);
	while (cur_num_reads < max_read && file.tellg() != -1L)
	{
		if (file.get(&c_pair[0], sizeof c_pair, null_char)) // overloaded get() to allow retrieval of newline & carriage return characters
		{
			 // if read == white space && peek (next char) != white space -> push into deque obj
			if (std::isspace(c_pair[0]) != 0  && std::isspace(file.peek()) == 0)
			{
				pos_que.push_back(cur_pos+1); // +1 as cur_pos pointed at white space character
				g_con_var.notify_one(); // notify writing thread
				++cur_num_reads; // increment to keep track of number of reads
				cur_pos -= 3L;
			} else
				--cur_pos;
		} else
		{
			CONSOLE_LOG << "get() failed at " << cur_num_reads << " number of previous reads\n";
			break;
		}	
		 // seek to new position
		file.seekg(cur_pos, std::ios_base::beg);
	}
	
	 // once max number of reverse file reads occurs -> push NAN to deque to stop extraction thread
	pos_que.push_back(std::nan("NAN"));	
	file.close(); // close file [read_files.at(1) via main]
}

template <size_t WA_SIZE, size_t DA_SIZE> // WA = write_array, DA = delims array
bool getMultipleDelim(std::ifstream& file, std::array<char, WA_SIZE>& write_array,
		const std::array<char, DA_SIZE>& delims_array, size_t& num_char_extracted)
{
	static auto l_checkDelims = [&](const char cur_char)->bool
	{
		for (size_t ind {0}; ind < DA_SIZE; ++ind)
		{
			if (cur_char == delims_array.at(ind))
				return true;
		}
		return false;
	};

	bool extraction_successful {false};
	static char c_hold {'A'};
	while (num_char_extracted < WA_SIZE && !l_checkDelims(file.peek()))
	{
		file.get(c_hold);
		write_array.at(num_char_extracted++) = c_hold;
		if (!extraction_successful)
			extraction_successful = true;
	}

	return extraction_successful;
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
	std::array<char, 17> temp_c_hold;
	temp_c_hold.fill('\n');
	constexpr std::array<char, 3> delim_array {' ', '\n', '\r'};
	size_t num_char_extracted {0};

	while (cur_vec_pos >= abs_stop_pos)
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
			num_char_extracted = 0; // set to 0 prior to modified get()
			// write to vector, starting from last index position
			if (getMultipleDelim<temp_c_hold.size(), delim_array.size()>
					(duplicate_RRF, temp_c_hold, delim_array, num_char_extracted)) // extract until white space or newline
			{
				char_to_double_stream.write(&temp_c_hold.at(0), num_char_extracted); // write to stringstream
				if (!(char_to_double_stream >> std::dec >> written_vector.at(cur_vec_pos--)))
				{
					CONSOLE_ERROR << "Failed string stream extraction.\n";
					break;
				}

				char_to_double_stream.clear(); // prior line will set failbit -> true (clear stream state flags)
				char_to_double_stream.str("");
				 // lock read thread from accessing deque while current thread pops off just-read stream position
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
