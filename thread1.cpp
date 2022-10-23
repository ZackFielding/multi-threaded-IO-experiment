#include <iostream>
#include <thread>
#include <string>
#include <array>
#include <fstream>
#include <memory>
#include <deque>
#include <cctype>
#include <cmath>
#include "thread1_funcs.hpp"

int main()
{
	std::array<std::ifstream, 2> read_streams;
	//std::ofstream file_to_read;

	std::string read_file {"test-read.txt"},
		write_file {"test-write.txt"};

	 // open first read-only file && get it's number count
	int* array_heap {nullptr};
	unsigned long long arr_size {};
	std::thread open_get_count_thread 
		([&](){
				read_streams.at(0).open(read_file);
				arr_size = getNumberCount(read_streams.at(0));
				array_heap = new int[arr_size];
			  });

	 // open second stream
	read_streams.at(1).open(read_file, std::ios_base::ate);

	open_get_count_thread.join(); // wait for non-main thread to finish

	if (!!read_streams.at(0) && !!read_streams.at(1))
	{
		 // compute max forward and reverse index reads (indexing into array_heap)
		 	// throws narrowing compiler warning -> OK, as that's purposeful in this case
		const long long max_read_fwd {std::ceil(static_cast<long double>(arr_size)/2)};
		const long long max_read_rev {max_read_fwd + 1L};	 

		std::thread from_start_thread ([&](){
			for(long long i {0}; i < max_read_fwd; ++i)
				if (!read_streams.at(0) >> array_heap[i])
				{
					std::cerr << "Failure in forward reading into heap array.\n";
					break;
				}
		});	

		reverseFileRead(
				arr_size,
				max_read_rev,
				read_streams.at(1),
				array_heap
						);	

		from_start_thread.join();
	}

	for (unsigned long long ind {0}; ind < arr_size; ++ind)
		std::cout << array_heap[ind] << ' ';

	delete [] array_heap;
	for (auto& rs : read_streams)
		rs.close();

	return 0;
}
