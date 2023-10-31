#pragma once

#include "Order.h"
#include <iostream>
#include <utility>
#include <memory>
#include <list>
#include <vector>
#include <random>
#include <chrono>
#include <barrier>
#include <thread>
#include <cstddef>

namespace OrderBook{

	template<typename OrderBookImpl>
	class OrderBookTesting{
	public:
		OrderBookTesting();

		void TestAll();
		void TestOneThread();
		void TestConcurrentThreads();

		void TestAddAndTop10();
		void TestEraseAndTop10();
		void TestChangeAndTop10();
		void TestInsertEraseChangeTop10();

		void ConcurrentTestAddAndTop10();
		void ConcurrentTestEraseAndTop10();
		void ConcurrentTestChangeAndTop10();

		std::vector<std::vector<size_t>> MeasureOneThread();

		size_t MeasureOneThreadAdd(size_t count, size_t half_range);
		size_t MeasureOneThreadRemove(size_t count, size_t half_range);
		size_t MeasureOneThreadChange(size_t count, size_t half_range);
		size_t MeasureOneThreadTop10(size_t count, size_t half_range);

		std::vector<std::vector<size_t>> MeasureConcurrentThreads();

		size_t MeasureConcurrentThreadsAdd(size_t count, size_t half_range);
		size_t MeasureConcurrentThreadsRemove(size_t count, size_t half_range);
		size_t MeasureConcurrentThreadsChange(size_t count, size_t half_range);
		size_t MeasureConcurrentThreadsTop10(size_t count, size_t half_range);
	
	
	private:
	
		Order GenerateRandomOrder(std::normal_distribution<double>& price_int_dist);
		std::vector<Order> GenerateOrders(size_t max_size, std::normal_distribution<double>& price_int_dist);
	
		template<template<typename> class Comparator>
		std::vector<Order> GetTop10(const std::vector<Order>& v_mix, Type type){
			Comparator<Order> comp{};
			std::vector<Order> v;
			v.reserve(v_mix.size());
			for(auto x: v_mix){
				if(x.type_ == type) v.push_back(x);
			}
			int v_size = static_cast<int>(v.size());
			int r_size = std::min(10, v_size);
			std::vector<Order> result;
			result.reserve(r_size);
			for(int i = 0; i < r_size; ++i){
				for(int j = i + 1; j < v_size; ++j){
					if(comp(v[j],v[i])) std::swap(v[j], v[i]);
				}
			}
			return {v.begin(), std::min(v.end(), v.begin()+10)};
		}
	
	private:

		std::random_device rd_;
		std::mt19937 mt_;
		std::uniform_int_distribution<uint64_t> id_dist_;
		std::uniform_int_distribution<size_t> count_dist_;
		std::uniform_int_distribution<size_t> price_frac_dist_;
	};


	template <typename OrderBookImpl>
	OrderBookTesting<OrderBookImpl>::OrderBookTesting(): mt_(rd_()), id_dist_(1,1e5),
						count_dist_(1,2e4), price_frac_dist_(0, 99){}

	template <typename OrderBookImpl>
	void OrderBookTesting<OrderBookImpl>::TestAll(){
		TestOneThread();
		TestConcurrentThreads();
	}

	template <typename OrderBookImpl>
	void OrderBookTesting<OrderBookImpl>::TestOneThread(){
		TestAddAndTop10();
		TestEraseAndTop10();
		TestChangeAndTop10();
		TestInsertEraseChangeTop10();
	}

	template <typename OrderBookImpl>
	void OrderBookTesting<OrderBookImpl>::TestConcurrentThreads(){
		ConcurrentTestAddAndTop10();
		ConcurrentTestEraseAndTop10();
		ConcurrentTestChangeAndTop10();
	}

	template <typename OrderBookImpl>
	void OrderBookTesting<OrderBookImpl>::TestAddAndTop10(){

		for(int i = 0; i < 1000; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			auto orders = GenerateOrders(1000, price_int_dist);
			std::vector<Order> top10 = GetTop10<std::greater>(orders, Type::BUY);
			std::list<Order> top10_bids_list{top10.begin(), top10.end()};
				top10 = GetTop10<std::less>(orders, Type::SELL);
				std::list<Order> top10_asks_list{top10.begin(), top10.end()};

				OrderBookImpl obj;

				unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
				std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));

				for(auto x: orders){
					obj.Add(x);
				}
				auto result = obj.ShowTop10();

				if((result.top10_bids_ != top10_bids_list) || (result.top10_asks_ != top10_asks_list)){
						std::cerr <<  "Add random both types 1000" << std::endl;
						return;

				}

			}

			std::cout << "TestAddAndTop10 passed!" << std::endl;
		}



	template <typename OrderBookImpl>
	void OrderBookTesting<OrderBookImpl>::TestEraseAndTop10(){

		for(int i = 0; i < 1000; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(1000, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));

			for(auto x: orders){
				obj.Add(x);
			}

			std::vector<bool> to_erase;
			to_erase.reserve(orders.size());
			std::bernoulli_distribution is_erased(std::uniform_real_distribution<>{0.0,1.0}(mt_));
			int size = static_cast<int>(orders.size());
			for(int i = 0; i < size; ++i){
				to_erase.push_back(is_erased(mt_));
			}

			std::vector<Order> new_orders;
			new_orders.reserve(size);
			for(int i = 0; i < size; ++i){
				if(to_erase[i]){
					obj.Remove(orders[i].id_.val_);
				}else{
					new_orders.push_back(orders[i]);
				}
			}


			std::vector<Order> top10 = GetTop10<std::greater>(new_orders, Type::BUY);
			std::list<Order> top10_bids_list{top10.begin(), top10.end()};
			top10 = GetTop10<std::less>(new_orders, Type::SELL);
			std::list<Order> top10_asks_list{top10.begin(), top10.end()};

			auto result = obj.ShowTop10();

			if((result.top10_asks_ !=  top10_asks_list) || (result.top10_bids_ != top10_bids_list)){
						std::cerr << "Erase random bids 1000" << std::endl;
						return;
			}

		}



		std::cout << "TestEraseAndTop10 passed!" << std::endl;
	}


	template <typename OrderBookImpl>
	void OrderBookTesting<OrderBookImpl>::TestChangeAndTop10(){

		for(int i = 0; i < 1000; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(1000, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));

			for(auto x: orders){
				obj.Add(x);
			}

			std::vector<bool> to_change;
			to_change.reserve(orders.size());
			std::bernoulli_distribution is_changed(std::uniform_real_distribution<>{0.0,1.0}(mt_));
			int size = static_cast<int>(orders.size());
			for(int i = 0; i < size; ++i){
				to_change.push_back(is_changed(mt_));
			}

			std::vector<Order> new_orders;
			new_orders.reserve(size);
			for(int i = 0; i < size; ++i){
				if(to_change[i]){
					Order changed_order = GenerateRandomOrder(price_int_dist);
					changed_order.id_ = orders[i].id_;
					new_orders.push_back(changed_order);
					obj.Change(changed_order);
				}else{
					new_orders.push_back(orders[i]);
				}
			}


			std::vector<Order> top10 = GetTop10<std::greater>(new_orders, Type::BUY);
			std::list<Order> top10_bids_list{top10.begin(), top10.end()};
			top10 = GetTop10<std::less>(new_orders, Type::SELL);
			std::list<Order> top10_asks_list{top10.begin(), top10.end()};

			auto result = obj.ShowTop10();

			if((result.top10_asks_ !=  top10_asks_list) || (result.top10_bids_ != top10_bids_list)){
						std::cerr << "Change random bids 1000" << std::endl;
						return;
			}

		}

		std::cout << "TestChangeAndTop10 passed!" << std::endl;
	}

	template <typename OrderBookImpl>
	void OrderBookTesting<OrderBookImpl>::TestInsertEraseChangeTop10(){

		for(int i = 0; i < 1000; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(10000, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));

			int size = static_cast<int>(orders.size());
			int old_size = size/2;
			std::vector<Order> new_orders;
			new_orders.reserve(size);
			for(int i = 0; i < old_size; ++i){
				obj.Add(orders[i]);
				new_orders.push_back(orders[i]);
			}

			size = std::min(size, static_cast<int>(new_orders.size()));
			for(int i = old_size; i < size; ++i){

				std::bernoulli_distribution perform_op(std::uniform_real_distribution<>{0.0,1.0}(mt_));

				if(perform_op(mt_)){
					new_orders.push_back(orders[i]);
					obj.Add(orders[i]);
				}

				if(perform_op(mt_)){
					std::uniform_int_distribution<int> dist(0,static_cast<int>(new_orders.size()) - 1);
					auto It = new_orders.begin() + dist(mt_);
					Order changed_order = GenerateRandomOrder(price_int_dist);
					changed_order.id_ = It->id_;
					obj.Change(changed_order);
					new_orders.erase(It);
					new_orders.push_back(std::move(changed_order));
				}

				if(perform_op(mt_)){
					std::uniform_int_distribution<int> dist(0,static_cast<int>(new_orders.size()) - 1);
					auto It = new_orders.begin() + dist(mt_);
					obj.Remove((*It).id_.val_);
					new_orders.erase(It);
				}

				std::vector<Order> top10 = GetTop10<std::greater>(new_orders, Type::BUY);
				std::list<Order> top10_bids_list{top10.begin(), top10.end()};
				top10 = GetTop10<std::less>(new_orders, Type::SELL);
				std::list<Order> top10_asks_list{top10.begin(), top10.end()};

				auto result = obj.ShowTop10();

				if((result.top10_asks_ !=  top10_asks_list) || (result.top10_bids_ != top10_bids_list)){
							std::cerr << "Insert&&Change&&Erase random bids. Wrong result." << std::endl;
							return;
				}
			}

		}

		std::cout << "TestInsertEraseChangeTop10 passed!" << std::endl;
	}


	template <typename OrderBookImpl>
	void OrderBookTesting<OrderBookImpl>::ConcurrentTestAddAndTop10(){

		for(int i = 0; i < 1000; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			auto orders = GenerateOrders(10000, price_int_dist);
			std::vector<Order> top10 = GetTop10<std::greater>(orders, Type::BUY);
			std::list<Order> top10_bids_list{top10.begin(), top10.end()};
			top10 = GetTop10<std::less>(orders, Type::SELL);
			std::list<Order> top10_asks_list{top10.begin(), top10.end()};

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));


			int t_count = std::max(2, static_cast<int>(std::thread::hardware_concurrency()));
			std::barrier sync_point(t_count);
			std::vector<std::thread> threads;
			threads.reserve(t_count);
			for(int t = 0; t < t_count; ++t){
				threads.emplace_back([&orders,&obj,&sync_point,t,t_count](){
					int size = static_cast<int>(orders.size());
					sync_point.arrive_and_wait();
					for(int i = t; i <  size; i+=t_count){
						obj.Add(orders[i]);
						//std::cout << std::this_thread::get_id() << std::endl;
					}
				});
			}

			for(auto& t: threads){
				t.join();
			}

			auto result = obj.ShowTop10();

			if((result.top10_bids_ != top10_bids_list) || (result.top10_asks_ != top10_asks_list)){
					std::cerr <<  "Concurrent Insert random both types 1000" << std::endl;
					return;

			}

		}

		std::cout << "ConccurentTestAddAndTop10 passed!" << std::endl;
	}


	template <typename OrderBookImpl>
	void OrderBookTesting<OrderBookImpl>::ConcurrentTestEraseAndTop10(){

		for(int i = 0; i < 1000; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(1000, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));

			for(auto x: orders){
				obj.Add(x);
			}

			int t_count = std::max(2, static_cast<int>(std::thread::hardware_concurrency()));
			std::barrier sync_point(t_count);
			std::vector<std::thread> threads;
			threads.reserve(t_count);
			for(int t = 0; t < t_count; ++t){
				threads.emplace_back([&orders,&obj,&sync_point,t,t_count](){
					int size = static_cast<int>(orders.size());
					sync_point.arrive_and_wait();
					for(int i = t; i <  size; i+=t_count){
						obj.Remove(orders[i].id_.val_);
					}
				});
			}

			for(auto& t: threads){
				t.join();
			}

			auto result = obj.ShowTop10();

			if((result.top10_asks_.size() !=  0u) || (result.top10_bids_.size() != 0u)){
						std::cerr << "Concurrent Erase random bids 1000" << std::endl;
						return;
			}

		}

		std::cout << "ConcurrentTestEraseAndTop10 passed!" << std::endl;
	}


	template <typename OrderBookImpl>
	void OrderBookTesting<OrderBookImpl>::ConcurrentTestChangeAndTop10(){

		for(int i = 0; i < 1000; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(1000, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));

			for(auto x: orders){
				obj.Add(x);
			}

			int t_count = std::max(2, static_cast<int>(std::thread::hardware_concurrency()));
			std::barrier sync_point(t_count);
			std::vector<std::thread> threads;
			threads.reserve(t_count);
			for(int t = 0; t < t_count; ++t){
				threads.emplace_back([&orders,&obj,&sync_point,t,t_count](){
					int size = static_cast<int>(orders.size());
					sync_point.arrive_and_wait();
					for(int i = t; i <  size; i+=t_count){
						orders[i].price_ = std::move(Price(static_cast<uint64_t>(t),10));
						obj.Change(orders[i]);
					}
				});
			}

			for(auto& t: threads){
				t.join();
			}


			std::vector<Order> top10 = GetTop10<std::greater>(orders, Type::BUY);
			std::list<Order> top10_bids_list{top10.begin(), top10.end()};
			top10 = GetTop10<std::less>(orders, Type::SELL);
			std::list<Order> top10_asks_list{top10.begin(), top10.end()};

			auto result = obj.ShowTop10();

			if((result.top10_asks_ !=  top10_asks_list) || (result.top10_bids_ != top10_bids_list)){
						std::cerr << "Concurrent Change random bids 1000" << std::endl;
						return;
			}

		}

		std::cout << "ConcurrentTestChangeAndTop10 passed!" << std::endl;
	}


	template <typename OrderBookImpl>
	std::vector<std::vector<size_t>> OrderBookTesting<OrderBookImpl>::MeasureOneThread(){
		OrderBookImpl obj;

		std::vector<size_t> elements_count = {1000, 5000, 10000, 50000};
		std::vector<std::vector<size_t>> result;
		result.resize(4);

		for(size_t count: elements_count){
			result[0].push_back(MeasureOneThreadAdd(count, 500));
			result[1].push_back(MeasureOneThreadRemove(count, 500));
			result[2].push_back(MeasureOneThreadChange(count, 500));
			result[3].push_back(MeasureOneThreadTop10(count, 500));
		}

		return result;
	}

	template <typename OrderBookImpl>
	size_t OrderBookTesting<OrderBookImpl>::MeasureOneThreadAdd(size_t count, size_t half_range){
		size_t sum_time = 0;
		for(int i = 0; i < 1; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(count+half_range, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));


			for(size_t i = 0; i < count - half_range; ++i){
				obj.Add(orders[i]);
			}

			auto start = std::chrono::high_resolution_clock::now();
			for(size_t i = count - half_range; i < count + half_range; ++i){
				obj.Add(orders[i]);
			}
			auto stop = std::chrono::high_resolution_clock::now();


			sum_time += std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
		}
		return sum_time/2/half_range;
	}


	template <typename OrderBookImpl>
	size_t OrderBookTesting<OrderBookImpl>::MeasureOneThreadRemove(size_t count, size_t half_range){
		size_t sum_time = 0;
		for(int i = 0; i < 1; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(count+half_range, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));


			for(size_t i = 0; i < count + half_range; ++i){
				obj.Add(orders[i]);
			}

			seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));

			auto start = std::chrono::high_resolution_clock::now();
			for(size_t i = 0; i < 2*half_range; ++i){
				obj.Remove(orders[i].id_.val_);
			}
			auto stop = std::chrono::high_resolution_clock::now();
			sum_time += std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
		}
		return sum_time/2/half_range;
	}


	template <typename OrderBookImpl>
	size_t OrderBookTesting<OrderBookImpl>::MeasureOneThreadChange(size_t count, size_t half_range){
		size_t sum_time = 0;
		for(int i = 0; i < 1; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(count+half_range, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));


			for(size_t i = 0; i < count + half_range; ++i){
				obj.Add(orders[i]);
			}

			seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));
			for(size_t i = 0; i < 2*half_range; ++i){
				orders[i].price_ = Price(i,10);
			}

			auto start = std::chrono::high_resolution_clock::now();
			for(size_t i = 0; i < 2*half_range; ++i){
				obj.Change(orders[i]);
			}
			auto stop = std::chrono::high_resolution_clock::now();
			sum_time += std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
		}
		return sum_time/2/half_range;
	}


	template <typename OrderBookImpl>
	size_t OrderBookTesting<OrderBookImpl>::MeasureOneThreadTop10(size_t count, size_t half_range){
		size_t sum_time = 0;
		for(int i = 0; i < 1; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(count+half_range, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));


			for(size_t i = 0; i < count + half_range; ++i){
				obj.Add(orders[i]);
			}

			auto start = std::chrono::high_resolution_clock::now();
			for(size_t i = 0; i < 2*half_range; ++i){
				obj.ShowTop10();
			}
			auto stop = std::chrono::high_resolution_clock::now();
			sum_time += std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
		}
		return sum_time/2/half_range;
	}




	template <typename OrderBookImpl>
	std::vector<std::vector<size_t>> OrderBookTesting<OrderBookImpl>::MeasureConcurrentThreads(){
		OrderBookImpl obj;

		std::vector<size_t> elements_count = {1000, 5000, 10000, 50000};
		std::vector<std::vector<size_t>> result;
		result.resize(4);

		for(size_t count: elements_count){
			result[0].push_back(MeasureConcurrentThreadsAdd(count, 500));
			result[1].push_back(MeasureConcurrentThreadsRemove(count, 500));
			result[2].push_back(MeasureConcurrentThreadsChange(count, 500));
			result[3].push_back(MeasureConcurrentThreadsTop10(count, 500));
		}

		return result;
	}


	template <typename OrderBookImpl>
	size_t OrderBookTesting<OrderBookImpl>::MeasureConcurrentThreadsAdd(size_t count, size_t half_range){
		std::atomic<size_t> sum_time{0};
		for(int i = 0; i < 1; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(count+half_range, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));


			for(size_t i = 0; i < count - half_range; ++i){
				obj.Add(orders[i]);
			}


			int t_count = std::max(2, static_cast<int>(std::thread::hardware_concurrency()));
			std::barrier sync_point(t_count);
			std::vector<std::thread> threads;
			threads.reserve(t_count);
			for(int t = 0; t < t_count; ++t){
				threads.emplace_back([&sum_time,&orders,&obj,&sync_point,t,t_count, count, half_range](){
					sync_point.arrive_and_wait();
					auto start = std::chrono::high_resolution_clock::now();
					for(size_t i = count - half_range+t; i < count + half_range; i+=t_count){
						obj.Add(orders[i]);
					}
					auto stop = std::chrono::high_resolution_clock::now();
					sum_time.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
				});
			}

			for(auto& t: threads){
				t.join();
			}

		}
		return sum_time.load()/2/half_range;
	}


	template <typename OrderBookImpl>
	size_t OrderBookTesting<OrderBookImpl>::MeasureConcurrentThreadsRemove(size_t count, size_t half_range){
		std::atomic<size_t> sum_time{0};
		for(int i = 0; i < 1; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(count+half_range, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));


			for(size_t i = 0; i < count + half_range; ++i){
				obj.Add(orders[i]);
			}

			seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));

			int t_count = std::max(2, static_cast<int>(std::thread::hardware_concurrency()));
			std::barrier sync_point(t_count);
			std::vector<std::thread> threads;
			threads.reserve(t_count);
			for(int t = 0; t < t_count; ++t){
				threads.emplace_back([&sum_time,&orders,&obj,&sync_point,t,t_count, count, half_range](){
					sync_point.arrive_and_wait();
					auto start = std::chrono::high_resolution_clock::now();
					for(size_t i = t; i < 2*half_range; i+=t_count){
						obj.Remove(orders[i].id_.val_);
					}
					auto stop = std::chrono::high_resolution_clock::now();
					sum_time.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
				});
			}

			for(auto& t: threads){
				t.join();
			}

		}
		return sum_time/2/half_range;
	}


	template <typename OrderBookImpl>
	size_t OrderBookTesting<OrderBookImpl>::MeasureConcurrentThreadsChange(size_t count, size_t half_range){
		std::atomic<size_t> sum_time{0};
		for(int i = 0; i < 1; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(count+half_range, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));


			for(size_t i = 0; i < count + half_range; ++i){
				obj.Add(orders[i]);
			}

			seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));
			for(size_t i = 0; i < 2*half_range; ++i){
				orders[i].price_ = Price(i,10);
			}



			int t_count = std::max(2, static_cast<int>(std::thread::hardware_concurrency()));
			std::barrier sync_point(t_count);
			std::vector<std::thread> threads;
			threads.reserve(t_count);
			for(int t = 0; t < t_count; ++t){
				threads.emplace_back([&sum_time,&orders,&obj,&sync_point,t,t_count, count, half_range](){
					sync_point.arrive_and_wait();
					auto start = std::chrono::high_resolution_clock::now();
					for(size_t i = t; i < 2*half_range; i+=t_count){
						obj.Change(orders[i]);
					}
					auto stop = std::chrono::high_resolution_clock::now();
					sum_time.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
				});
			}

			for(auto& t: threads){
				t.join();
			}

		}
		return sum_time/2/half_range;
	}


	template <typename OrderBookImpl>
	size_t OrderBookTesting<OrderBookImpl>::MeasureConcurrentThreadsTop10(size_t count, size_t half_range){
		std::atomic<size_t> sum_time{0};
		for(int i = 0; i < 1; ++i){
			std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
			std::vector orders = GenerateOrders(count+half_range, price_int_dist);

			OrderBookImpl obj;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));


			for(size_t i = 0; i < count + half_range; ++i){
				obj.Add(orders[i]);
			}


			int t_count = std::max(2, static_cast<int>(std::thread::hardware_concurrency()));
			std::barrier sync_point(t_count);
			std::vector<std::thread> threads;
			threads.reserve(t_count);
			for(int t = 0; t < t_count; ++t){
				threads.emplace_back([&sum_time,&orders,&obj,&sync_point,t,t_count, count, half_range](){
					sync_point.arrive_and_wait();
					auto start = std::chrono::high_resolution_clock::now();
					for(size_t i = t; i < 2*half_range; i+=t_count){
						obj.ShowTop10();
					}
					auto stop = std::chrono::high_resolution_clock::now();
					sum_time.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
				});
			}

			for(auto& t: threads){
				t.join();
			}

		}
		return sum_time/2/half_range;
	}


	template <typename OrderBookImpl>
	Order OrderBookTesting<OrderBookImpl>::GenerateRandomOrder(std::normal_distribution<double>& price_int_dist){
		std::bernoulli_distribution type_dist(std::uniform_real_distribution<>{0.0,1.0}(mt_));
		return Order(Id(id_dist_(mt_)), Price(static_cast<size_t>(std::abs(price_int_dist(mt_))), price_frac_dist_(mt_)),
															Count(count_dist_(mt_)), static_cast<Type>(type_dist(mt_)));
	}

	template <typename OrderBookImpl>
	std::vector<Order> OrderBookTesting<OrderBookImpl>::GenerateOrders(size_t max_size, std::normal_distribution<double>& price_int_dist){

		std::vector<Order> v;
		v.reserve(max_size);

		for(size_t i = 0; i < max_size; ++i){
			v.push_back(GenerateRandomOrder(price_int_dist));
		}

		std::sort(v.begin(),v.end(),[](const Order& left, const Order& right){
			return left.id_ < right.id_;
		});

		auto It = std::unique(v.begin(), v.end(), [](const Order& left, const Order& right){
			return left.id_ == right.id_;
		});

		v.erase(It, v.end());

		return v;
	}


}
