
#include "ConcurrentOrderBook_HashSet.h"

namespace OrderBook{

	bool ConcurrentOrderBook_HashSet::Add(Order order){
		bool success = false;
		uint64_t id = order.id_.val_;
		if(order.type_ == Type::BUY){
			Pair_bid res = bids_.Add(std::move(order));
			success = res.second;
			if(success){
				success = orders_bids_.insert({id, res.first});
			}
		}else if(order.type_ == Type::SELL){
			Pair_ask res = asks_.Add(std::move(order));
			success = res.second;
			if(success){
				success = orders_asks_.insert({id, res.first});
			}
		}
		
		return success;
	}
	
	void ConcurrentOrderBook_HashSet::Remove(uint64_t id){

		OrderStorage_bids::accessor b;
		bool success = orders_bids_.find(b, id);
		if(success) {
			bids_.Remove(b->second);
			orders_bids_.erase(b);
			return;
		}

		OrderStorage_asks::accessor a;
		success = orders_asks_.find(a, id);
		if(success) {
			asks_.Remove(a->second);
			orders_asks_.erase(a);
			return;
		}
	}
	
	bool ConcurrentOrderBook_HashSet::Change(Order order){
		Remove(order.id_.val_);
		return Add(std::move(order));
	}


	
	ConcurrentOrderBook_HashSet::BothTop10 ConcurrentOrderBook_HashSet::ShowTop10() const{
		return {bids_.ShowTop10(),asks_.ShowTop10()};
	}


}
