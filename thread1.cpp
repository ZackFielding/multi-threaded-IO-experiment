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
#include "thread1_funcs.hpp"

int main()
{
	 // array of read-only text files
	std::array<std::ifstream, 2> read_streams;
	std::string read_file {"test-read.txt"};

	 // open first read-only file && get it's number count
	std::vector<int> written_vector;
	std::thread open_get_count_thread 
		([](std::ifstream& file){
				file.open(read_file);
				written_vector.resize(getNumberCount(file)); // re-size vector
			  }, std::ref(read_streams.at(0)), std::ref(written_vector));

	read_streams.at(1).open(read_file, std::ios_base::ate); // open second stream
	open_get_count_thread.join(); // wait for non-main thread to finish

	if (!!read_streams.at(0) && !!read_streams.at(1))
	{
		 // compute max forward and reverse index reads 
		if (written_vector % 2 != 0) // if size is odd
			const double max_read_fwd {written_vector.size() / 2 + 1};
		else
			const double max_read_fwd {written_vector.size() / 2};

		std::thread from_start_thread ([=](std::ifstream& file, std::vector<int> written_vector){
			for(double fwd_ind {0.0}; fwd_ind < max_read_fwd; ++fwd_ind)
			{
				if (!file >> written_vector.at(fwd_ind))
				{
					CONSOLE_ERROR << "Failure in forward reading into heap array.\n";
					break;
				}
				else
				{
					CONSOLE_LOG << "read from secondary thread.. ";
				}
			}
		}, std::ref(read_streams.at(0)), std::ref(written_vector));	

		reverseFileRead(read_streams.at(1), written_vector);	

		from_start_thread.join();
		read_streams.at(0).close();
	}
	else
	{
		std::clog << "Streams failed to open.\n";
	}

	std::clog << "Array test: " << array_heap[0];
	for (unsigned long long ind {0}; ind < arr_size; ++ind)
		CONSOLE_OUT << array_heap[ind] << ' ';

	delete [] array_heap;
	for (auto& rs : read_streams)
		rs.close();

	return 0;
}
