#pragma once

#include "Order.h"
#include "oneapi/tbb/concurrent_set.h"
#include "oneapi/tbb/concurrent_hash_map.h"
#include <utility>
#include <memory>
#include <set>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace OrderBook{

	namespace tbb = oneapi::tbb;
	
	class ConcurrentOrderBook_HashMap{
	public:
		using OrderStorage = tbb::concurrent_hash_map<uint64_t, Order>;

		bool Add(Order order);
		bool Remove(uint64_t id);
		bool Change(Order order);

		struct BothTop10{
			std::list<Order> top10_bids_;
			std::list<Order> top10_asks_;
		};

		BothTop10 ShowTop10() const;

	private:

		template<typename Comparator>
		std::list<Order> FindTop10(std::vector<Order>& v) const;
		
	private:
		OrderStorage bids_;
		OrderStorage asks_;
		mutable std::shared_mutex sh_mutex_b;
		mutable std::shared_mutex sh_mutex_a;
	
	
	};

	template<typename Comparator>
	std::list<Order> ConcurrentOrderBook_HashMap::FindTop10(std::vector<Order>& v) const{
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

}
