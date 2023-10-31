#include <iostream>
#include "ConcurrentOrderBook_HashSet.h"
#include "ConcurrentOrderBook_HashMap.h"
#include "TestClass.h"

int main() {

	OrderBook::OrderBookTesting<OrderBook::ConcurrentOrderBook_HashSet> test;
    test.TestAll();

    auto result = test.MeasureOneThread();

    std::cout << "Add:\n";
	for(auto x: result[0]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "Remove:\n";
	for(auto x: result[1]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "Change:\n";
	for(auto x: result[2]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "Top10:\n";
	for(auto x: result[3]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	auto result1 = test.MeasureConcurrentThreads();
	std::cout << "ConcurrentAdd:\n";
	for(auto x: result1[0]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "ConcurrentRemove:\n";
	for(auto x: result1[1]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "ConcurrentChange:\n";
	for(auto x: result1[2]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "ConcurrentTop10:\n";
	for(auto x: result1[3]){
		std::cout << x << " ";
	}
	std::cout << std::endl;
	
	/////////////////////////////////
	

	OrderBook::OrderBookTesting<OrderBook::ConcurrentOrderBook_HashMap> test1;
	test1.TestAll();
	
	result = test1.MeasureOneThread();

	std::cout << "Add:\n";
	for(auto x: result[0]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "Remove:\n";
	for(auto x: result[1]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "Change:\n";
	for(auto x: result[2]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "Top10:\n";
	for(auto x: result[3]){
		std::cout << x << " ";
	}
	std::cout << std::endl;


	result1 = test1.MeasureConcurrentThreads();
	std::cout << "ConcurrentAdd:\n";
	for(auto x: result1[0]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "ConcurrentRemove:\n";
	for(auto x: result1[1]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "ConcurrentChange:\n";
	for(auto x: result1[2]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	std::cout << "ConcurrentTop10:\n";
	for(auto x: result1[3]){
		std::cout << x << " ";
	}
	std::cout << std::endl;

	return 0;
}
