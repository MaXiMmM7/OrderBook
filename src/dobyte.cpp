#include <iostream>
#include <utility>
#include <memory>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <cassert>
#include <vector>
#include <random>
#include <chrono>
#include <barrier>
#include <thread>
#include <atomic>
#include <iterator>
#include <set>
#include "oneapi/tbb/concurrent_set.h"
#include "oneapi/tbb/concurrent_hash_map.h"

namespace tbb = oneapi::tbb;
//using namespace oneapi::tbb;


enum Type{
	SELL = 0,
	BUY = 1
};



struct Id{
	Id(){}
	explicit Id(uint64_t id): val_(id){}
	uint64_t val_ = 0;
};

bool operator==(const Id& left, const Id& right){
	return  left.val_== right.val_;
}

bool operator<(const Id& left, const Id& right){
	return left.val_ < right.val_;
}

bool operator>(const Id& left, const Id& right){
	return left.val_ > right.val_;
}



struct Price{
	Price(){}
	explicit Price(size_t int_p, size_t frac_p): integer_part_(int_p), fractional_part_(frac_p){
		if(fractional_part_ > 99) throw std::runtime_error("Invalid fractional part.");
	}
	size_t integer_part_;
	size_t fractional_part_;
};


bool operator==(const Price& left, const Price& right){
	return  left.integer_part_== right.integer_part_ &&  left.fractional_part_== right.fractional_part_;
}

bool operator<(const Price& left, const Price& right){
	return left.integer_part_ < right.integer_part_ ?  true :
			(left.integer_part_ == right.integer_part_ ? left.fractional_part_ < right.fractional_part_ : false);
}

bool operator>(const Price& left, const Price& right){
	return left.integer_part_ > right.integer_part_ ? true :
			 (left.integer_part_ == right.integer_part_ ? left.fractional_part_ > right.fractional_part_: false);
}


struct Count{
	Count(){}
	explicit Count(size_t count): val_(count){}
	size_t val_;
};



bool operator==(const Count& left, const Count& right){
	return  left.val_== right.val_;
}

bool operator<(const Count& left, const Count& right){
	return left.val_ < right.val_;
}

bool operator>(const Count& left, const Count& right){
	return left.val_ > right.val_;
}


struct Order{
	Order(){}
	explicit Order(Id id, Price price, Count count, Type type): id_(id), price_(price), count_(count), type_(type){}
	Id id_;
	Price price_;
	Count count_;
	Type type_;
};


std::ostream& operator<<(std::ostream& output, const Order& order){
    output  << "id = " << order.id_.val_ << " price = " << order.price_.integer_part_ << "." <<
    order.price_.fractional_part_ << " count = " << order.count_.val_ << " type = " << order.type_ << std::endl;
    return output;
}


bool operator==(const Order& left, const Order& right){
	return left.price_ == right.price_ && left.count_ == right.count_ && left.id_== right.id_;
}

bool operator<(const Order& left, const Order& right){
	if(left.price_ < right.price_){
		return true;
	}else if(left.price_ == right.price_){
		return left.count_ > right.count_ ? true : (left.count_ == right.count_ ? left.id_ > right.id_ : false);
	}
	return false;
}

bool operator>(const Order& left, const Order& right){
	if(left.price_ > right.price_){
		return true;
	}else if(left.price_ == right.price_){
		return left.count_ > right.count_ ? true : (left.count_ == right.count_ ? left.id_ > right.id_ : false);
	}
	return false;
}





class ConcurrentOrderBook_VectorFixSize{
public:
	using Map = tbb::concurrent_hash_map<uint64_t, Type>;
	using OrderStorage = tbb::concurrent_vector<Order>;


	ConcurrentOrderBook_VectorFixSize(size_t size = 100000){
		bids_.resize(size);
		asks_.resize(size);
	}
	
	void Add(Order order){

		size_t id = static_cast<size_t>(order.id_.val_);
		map_.insert({id, order.type_});
		if(order.type_ == Type::BUY){
			bids_[id] = std::move(order);
		}else if(order.type_ == Type::SELL){
			asks_[id] = std::move(order);
		}
		//std::cout << " size map = " << map_.size() << std::endl;
	}
	
	void Remove(uint64_t id){

		size_t id_s = static_cast<size_t>(id);
		Map::accessor a;
		if(!map_.find(a, id)) return;

		if( a->second == Type::BUY){
			bids_[id_s].id_.val_ = 0;
		}else{
			asks_[id_s].id_.val_ = 0;
		}
		map_.erase(a);
	}
	
	void Change(Order order){
		Remove(order.id_.val_);
		Add(std::move(order));
	}

	struct BothTop10{
		std::list<Order> top10_bids_;
		std::list<Order> top10_asks_;
	};

	BothTop10 ShowTop10() const{
		std::vector<Order> copy_b;
		std::vector<Order> copy_a;
		size_t size;
		{
			size = bids_.size();
			copy_b.reserve(size);
			for(auto It = bids_.begin(); It != bids_.end(); ++It){
				//std::cout << *It;
				if(It->id_.val_ != 0){
					//std::cout << *It;
					copy_b.push_back(*It);
				}
			}
		}
		{
			size = asks_.size();
			copy_a.reserve(size);
			for(auto It = asks_.begin(); It != asks_.end(); ++It){
				if(It->id_.val_ != 0) copy_a.push_back(*It);
			}
		}

		return {FindTop10<std::greater<Order>>(copy_b),FindTop10<std::less<Order>>(copy_a)};
	}
private:
	template<typename Comparator>
	std::list<Order> FindTop10(std::vector<Order>& v) const{
		int size = static_cast<int>(v.size());
		Comparator Cmp;
		std::set<Order, Comparator> res;
		int i = 0;
		for(; i < std::min(10, size); ++i){
			res.insert(v[i]);
		}
		for(;i < size; ++i){
			if(Cmp(v[i],*std::prev(res.end()))){
				res.erase(std::prev(res.end()));
				res.insert(v[i]);
			}
		}
		return {res.begin(), res.end()};
	}
	
private:
	OrderStorage bids_;
	OrderStorage asks_;
	Map map_;
};


template<typename OrderBookImpl>
class OrderBookTesting{
public:
	OrderBookTesting(): mt_(rd_()), id_dist_(1,1e5), count_dist_(1,2e4), price_frac_dist_(0, 99){}//!!!!!!!

	void TestAll(){
		TestOneThread();
		TestConcurrentThreads();
	}


	void TestOneThread(){
		TestAddAndTop10();
		TestEraseAndTop10();
		TestChangeAndTop10();
		TestInsertEraseChangeTop10();
	}

	void TestConcurrentThreads(){
		ConcurrentTestAddAndTop10();
		ConcurrentTestEraseAndTop10();
		ConcurrentTestChangeAndTop10();
	}

	void TestAddAndTop10(){


		{

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


				//////////////////
/*
				for(auto x: result.top10_bids_){
					std::cout << x;
				}
				std::cout << "||||||||||||||||||||\n";

				for(auto x: top10_bids_list){
					std::cout << x;
				}
				std::cout << "\n\n\n\n\n\n" << std::endl;
*/
				////////////
				if((result.top10_bids_ != top10_bids_list) || (result.top10_asks_ != top10_asks_list)){
						std::cerr <<  "Add random both types 1000" << std::endl;
						return;

				}

			}

		}

		std::cout << "TestAddAndTop10 passed!" << std::endl;
	}




	void TestEraseAndTop10(){
		{

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

		}

		std::cout << "TestEraseAndTop10 passed!" << std::endl;
	}

	void TestChangeAndTop10(){
		{

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

		}

		std::cout << "TestChangeAndTop10 passed!" << std::endl;
	}

	void TestInsertEraseChangeTop10(){
		{

			for(int i = 0; i < 1000; ++i){
				std::normal_distribution<double> price_int_dist(static_cast<double>(count_dist_(mt_)), static_cast<double>(count_dist_(mt_)));
				std::vector orders = GenerateOrders(10000, price_int_dist);

				OrderBookImpl obj;

				unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
				std::shuffle(orders.begin(), orders.end(), std::default_random_engine(seed));

				/////////
				int size = static_cast<int>(orders.size());
				int old_size = size/2;
				std::vector<Order> new_orders;
				new_orders.reserve(size);
				for(int i = 0; i < old_size; ++i){
					obj.Add(orders[i]);
					new_orders.push_back(orders[i]);
				}



				 /////////////////////
				/*

				std::cout <<  "prev##########################\n";
					for(auto x: orders){
						std::cout << x << " ";
					}
					std::cout << std::endl;

					for(auto x: new_orders){
						std::cout << x << " ";
					}
					std::cout <<  "prev##########################\n";
					std::cout << "\n\n\n\n\n";
*/
					///////////////////////


				size = std::min(size, static_cast<int>(new_orders.size()));
				for(int i = old_size; i < size; ++i){
					//std::cout << "i = " << i << " size = " << size << std::endl;

					std::bernoulli_distribution perform_op(std::uniform_real_distribution<>{0.0,1.0}(mt_));

					if(perform_op(mt_)){
						new_orders.push_back(orders[i]);
						obj.Add(orders[i]);
						//std::cout << "Add code = " << obj.Add(orders[i]) << std::endl;
						//std::cout << orders[i];
					}

					if(perform_op(mt_)){
						std::uniform_int_distribution<int> dist(0,static_cast<int>(new_orders.size()) - 1);
						auto It = new_orders.begin() + dist(mt_);
						Order changed_order = GenerateRandomOrder(price_int_dist);
						changed_order.id_ = It->id_;
						obj.Change(changed_order);
						//std::cout << "Change code = " << obj.Change(changed_order) << std::endl;
						//std::cout << changed_order;
						new_orders.erase(It);
						new_orders.push_back(std::move(changed_order));
					}
					
					if(perform_op(mt_)){
						std::uniform_int_distribution<int> dist(0,static_cast<int>(new_orders.size()) - 1);
						auto It = new_orders.begin() + dist(mt_);
						obj.Remove((*It).id_.val_);
						//std::cout << "Remove code = " << obj.Remove(*It)  << "*"<< std::endl;
						//std::cout << *It;
						new_orders.erase(It);
					}
					
					std::vector<Order> top10 = GetTop10<std::greater>(new_orders, Type::BUY);
					std::list<Order> top10_bids_list{top10.begin(), top10.end()};
					top10 = GetTop10<std::less>(new_orders, Type::SELL);
					std::list<Order> top10_asks_list{top10.begin(), top10.end()};

					auto result = obj.ShowTop10();
					 /////////////////////
/*
					 std::cout << "\n\n\n\n\n";

						 for(auto x: result.top10_bids_){
							 std::cout << x;
						 }
						 std::cout << "-------------------" << std::endl;
						 for(auto x: top10_bids_list){
							 std::cout << x;
						}

						 std::cout << "\n||||||||||||||||||||||||||||||||||||||||||||\n";

							 for(auto x: result.top10_asks_){
								 std::cout << x;
							 }
							 std::cout << "-------------------" << std::endl;
							 for(auto x: top10_asks_list){
								 std::cout << x;
							}
*/
						///////////////////////

					if((result.top10_asks_ !=  top10_asks_list) || (result.top10_bids_ != top10_bids_list)){
								std::cerr << "Insert&&Change&&Erase random bids. Wrong result." << std::endl;
								return;
					}
				}

			}

		}

		std::cout << "TestInsertEraseChangeTop10 passed!" << std::endl;
	}





public:

	void ConcurrentTestAddAndTop10(){


		{

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

		}

		std::cout << "ConccurentTestAddAndTop10 passed!" << std::endl;
	}



	void ConcurrentTestEraseAndTop10(){
		{

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
							//std::cout << std::this_thread::get_id() << std::endl;
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

		}

		std::cout << "ConcurrentTestEraseAndTop10 passed!" << std::endl;
	}

	void ConcurrentTestChangeAndTop10(){
		{

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
							//std::cout << std::this_thread::get_id() << std::endl;
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

		}

		std::cout << "ConcurrentTestChangeAndTop10 passed!" << std::endl;
	}


public:

	/*void MeasureAll(){
		MeasureOneThread();
		MeasureConcurrentThreads();
	}
	*/

	std::vector<std::vector<size_t>> MeasureOneThread(){
		OrderBookImpl obj;

		std::vector<size_t> elements_count = {1000, 5000, 10000, 50000};
		std::vector<std::vector<size_t>> result;
		result.resize(4);
		//
		//std::cout << "this one" << std::endl;
		//
		for(size_t count: elements_count){
			result[0].push_back(MeasureOneThreadAdd(count, 500));
			result[1].push_back(MeasureOneThreadRemove(count, 500));
			result[2].push_back(MeasureOneThreadChange(count, 500));
			result[3].push_back(MeasureOneThreadTop10(count, 500));
		}
		/*std::cout << "Add:\n";
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
		*/
		return result;

	}

	size_t MeasureOneThreadAdd(size_t count, size_t half_range){
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

	size_t MeasureOneThreadRemove(size_t count, size_t half_range){
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

	size_t MeasureOneThreadChange(size_t count, size_t half_range){
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



	size_t MeasureOneThreadTop10(size_t count, size_t half_range){
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






	std::vector<std::vector<size_t>> MeasureConcurrentThreads(){
		OrderBookImpl obj;

		std::vector<size_t> elements_count = {1000, 5000, 10000, 50000};
		std::vector<std::vector<size_t>> result;
		result.resize(4);
		//
		//std::cout << "THis One" << std::endl;
		//
		for(size_t count: elements_count){
			result[0].push_back(MeasureConcurrentThreadsAdd(count, 500));
			result[1].push_back(MeasureConcurrentThreadsRemove(count, 500));
			result[2].push_back(MeasureConcurrentThreadsChange(count, 500));
			result[3].push_back(MeasureConcurrentThreadsTop10(count, 500));
		}
		/*
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
*/
		return result;

	}

	size_t MeasureConcurrentThreadsAdd(size_t count, size_t half_range){
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
			//std::cout << "t_count  = " << t_count << std::endl;
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


	size_t MeasureConcurrentThreadsRemove(size_t count, size_t half_range){
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

	size_t MeasureConcurrentThreadsChange(size_t count, size_t half_range){
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



	size_t MeasureConcurrentThreadsTop10(size_t count, size_t half_range){
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


private:


	Order GenerateRandomOrder(std::normal_distribution<double>& price_int_dist){
		std::bernoulli_distribution type_dist(std::uniform_real_distribution<>{0.0,1.0}(mt_));
		return Order(Id(id_dist_(mt_)), Price(static_cast<size_t>(std::abs(price_int_dist(mt_))), price_frac_dist_(mt_)),
															Count(count_dist_(mt_)), static_cast<Type>(type_dist(mt_)));
	}


	std::vector<Order> GenerateOrders(size_t max_size, std::normal_distribution<double>& price_int_dist){

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
		//
		/*std::cout << "(It == v.end()) = " <<  (It == v.end()) << std::endl;
		//
		for(auto x: v){
			std::cout << x << " ";
		}
		std::cout << std::endl;
*/
		v.erase(It, v.end());
/*
		for(auto x: v){
			std::cout << x << " ";
		}
		std::cout << std::endl;
		*/

		return v;
	}

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

	
	std::random_device rd_;
	std::mt19937 mt_;
	std::uniform_int_distribution<uint64_t> id_dist_;
	std::uniform_int_distribution<size_t> count_dist_;
	std::uniform_int_distribution<size_t> price_frac_dist_;
};



int main() {
	ConcurrentOrderBook_VectorFixSize book_;
    OrderBookTesting<ConcurrentOrderBook_VectorFixSize> test;
    test.TestAll();
	auto start = std::chrono::high_resolution_clock::now();
    auto result = test.MeasureOneThread();
	auto stop = std::chrono::high_resolution_clock::now();
	std::cout << "TOTAL: " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();

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

	start = std::chrono::high_resolution_clock::now();
	auto result1 = test.MeasureConcurrentThreads();
	stop = std::chrono::high_resolution_clock::now();
	std::cout << "TOTAL: " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
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
