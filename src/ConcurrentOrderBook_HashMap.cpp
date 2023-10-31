
#include "ConcurrentOrderBook_HashMap.h"

namespace OrderBook{

	namespace tbb = oneapi::tbb;

	bool ConcurrentOrderBook_HashMap::Add(Order order){
	
		bool success = false;
		uint64_t id = order.id_.val_;
		if(order.type_ == Type::BUY){
			std::shared_lock lk(sh_mutex_b);
			success = bids_.insert({id, std::move(order)});
		}else if(order.type_ == Type::SELL){
			std::shared_lock lk(sh_mutex_a);
			success = asks_.insert({id, std::move(order)});
		}
		
		return success;
	}
	
	bool ConcurrentOrderBook_HashMap::Remove(uint64_t id){

		OrderStorage::accessor a;
		bool success = bids_.find(a, id);
		if(success) {
			std::shared_lock lk(sh_mutex_b);
			return bids_.erase(a);
		}
		a.release();
		success = asks_.find(a, id);
		if(success) {
			std::shared_lock lk(sh_mutex_a);
			return asks_.erase(a);
		}

		return success;
	}
	
	bool ConcurrentOrderBook_HashMap::Change(Order order){
		Remove(order.id_.val_);
		return Add(std::move(order));
	}



	ConcurrentOrderBook_HashMap::BothTop10 ConcurrentOrderBook_HashMap::ShowTop10() const{
		std::vector<Order> copy_b;
		std::vector<Order> copy_a;
		size_t size;
		{
			std::lock_guard lk_(sh_mutex_b);
			size = bids_.size();
			copy_b.reserve(size);
			for(auto It = bids_.begin(); It != bids_.end(); ++It){
				copy_b.push_back(It->second);
			}
		}
		{
			std::lock_guard lk_(sh_mutex_a);
			size = asks_.size();
			copy_a.reserve(size);
			for(auto It = asks_.begin(); It != asks_.end(); ++It){
				copy_a.push_back(It->second);
			}
		}

		return {FindTop10<std::greater<Order>>(copy_b),FindTop10<std::less<Order>>(copy_a)};
	}


}
