#pragma once


#include "Order.h"
#include "oneapi/tbb/concurrent_set.h"
#include "oneapi/tbb/concurrent_hash_map.h"
#include <utility>
#include <memory>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace OrderBook{

namespace tbb = oneapi::tbb;

	template<typename Comparator>
	class ConcurrentOrderedBookPrototype{
	public:

		using OrderBook  = tbb::concurrent_set<Order, Comparator>;
		using Pair = std::pair<typename OrderBook::iterator,bool>;

		Pair Add(Order order);
		void Remove(OrderBook::iterator It);
		Pair Change(OrderBook::iterator old_It, Order new_order);
		std::list<Order> ShowTop10() const;

	private:
		OrderBook order_book_;
		mutable std::shared_mutex sh_mutex_;
	};

	class ConcurrentOrderBook_HashSet{
	public:
		using OrderStorage_bids = tbb::concurrent_hash_map<uint64_t, typename tbb::concurrent_set<Order, std::greater<Order>>::iterator>;
		using OrderStorage_asks = tbb::concurrent_hash_map<uint64_t, typename tbb::concurrent_set<Order, std::less<Order>>::iterator>;
		using Pair_bid = std::pair<typename tbb::concurrent_set<Order, std::greater<Order>>::iterator,bool>;
		using Pair_ask = std::pair<typename tbb::concurrent_set<Order, std::less<Order>>::iterator,bool>;

		bool Add(Order order);

		void Remove(uint64_t id);
		bool Change(Order order);

		struct BothTop10{
			std::list<Order> top10_bids_;
			std::list<Order> top10_asks_;
		};

		BothTop10 ShowTop10() const;

	private:
		OrderStorage_bids orders_bids_;
		OrderStorage_asks orders_asks_;
		ConcurrentOrderedBookPrototype<std::less<Order>> asks_;
		ConcurrentOrderedBookPrototype<std::greater<Order>> bids_;
	};

	
	
	template<typename T>
	typename ConcurrentOrderedBookPrototype<T>::Pair
		ConcurrentOrderedBookPrototype<T>::Add(Order order){
		Pair result;
		{
			std::shared_lock lk(sh_mutex_);
			result = order_book_.insert(std::move(order));
		}
		return result;
	}
	
	template<typename T>
	void ConcurrentOrderedBookPrototype<T>::Remove(OrderBook::iterator It){
		{
			std::lock_guard lk(sh_mutex_);
			order_book_.unsafe_erase(It);
		}
	}

	template<typename T>
	ConcurrentOrderedBookPrototype<T>::Pair
		ConcurrentOrderedBookPrototype<T>::Change(OrderBook::iterator old_It, Order new_order){
		Remove(old_It);
		Pair result = Add(std::move(new_order));
		return result;
	}

	template<typename T>
	std::list<Order> ConcurrentOrderedBookPrototype<T>::ShowTop10() const{
		std::list<Order> top10;
		int counter = 0;
		{
			std::shared_lock lk(sh_mutex_);
			for(auto It = order_book_.begin();
					It != order_book_.end() && counter < 10; ++It, ++counter){
				top10.push_back(*It);
			}
		}
		return top10;
	}


}
