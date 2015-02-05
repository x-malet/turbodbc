#pragma once

#include <pydbc/field.h>
#include <boost/optional.hpp>

namespace pydbc {

/**
 * @brief This interface represents a single column of a result set.
 */
struct column {
	/**
	 * @brief Retrieve the field of the current result set row associated with this column
	 */
	nullable_field get_field() const;

	column(column const &) = delete;
	column & operator=(column const &) = delete;

	virtual ~column();
protected:
	column();
private:
	virtual nullable_field do_get_field() const = 0;
};


}