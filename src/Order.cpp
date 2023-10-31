#include "Order.h"
#include <stdexcept>

namespace OrderBook{

	Id::Id(){}
	Id::Id(uint64_t id): val_(id){}

	bool operator==(const Id& left, const Id& right){
		return  left.val_== right.val_;
	}

	bool operator<(const Id& left, const Id& right){
		return left.val_ < right.val_;
	}

	bool operator>(const Id& left, const Id& right){
		return left.val_ > right.val_;
	}



	Price::Price(){}
	Price::Price(size_t int_p, size_t frac_p): integer_part_(int_p), fractional_part_(frac_p){
		if(fractional_part_ > 99) throw std::runtime_error("Invalid fractional part.");
	}
	

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


	Count::Count(){}
	Count::Count(size_t count): val_(count){}



	bool operator==(const Count& left, const Count& right){
		return  left.val_== right.val_;
	}

	bool operator<(const Count& left, const Count& right){
		return left.val_ < right.val_;
	}
	
	bool operator>(const Count& left, const Count& right){
		return left.val_ > right.val_;
	}


	Order::Order(){}
	Order::Order(Id id, Price price, Count count, Type type): id_(id), price_(price), count_(count), type_(type){}

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
	
}

