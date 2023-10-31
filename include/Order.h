#pragma once
#include <cstddef>
#include <cstdint>

namespace OrderBook{

	enum Type{
		SELL = 0,
		BUY = 1
	};



	struct Id{
		Id();
		explicit Id(uint64_t id);
		uint64_t val_ = 0;;
	};

	bool operator==(const Id& left, const Id& right);
	bool operator<(const Id& left, const Id& right);
	bool operator>(const Id& left, const Id& right);


	struct Price{
		Price();
		explicit Price(size_t int_p, size_t frac_p);
		size_t integer_part_;
		size_t fractional_part_;
	};
	
	bool operator==(const Price& left, const Price& right);
	bool operator<(const Price& left, const Price& right);
	bool operator>(const Price& left, const Price& right);
	
	
	struct Count{
		Count();
		explicit Count(size_t count);
		size_t val_;
	};
	
	bool operator==(const Count& left, const Count& right);
	bool operator<(const Count& left, const Count& right);
	bool operator>(const Count& left, const Count& right);


	struct Order{
		Order();
		explicit Order(Id id, Price price, Count count, Type type);
		Id id_;
		Price price_;
		Count count_;
		Type type_;
	};
	
	bool operator==(const Order& left, const Order& right);
	bool operator<(const Order& left, const Order& right);
	bool operator>(const Order& left, const Order& right);

}

