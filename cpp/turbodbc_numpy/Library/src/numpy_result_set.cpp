#include <turbodbc_numpy/numpy_result_set.h>

#include <boost/python/list.hpp>
#include <boost/python/tuple.hpp>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
// compare http://docs.scipy.org/doc/numpy/reference/c-api.array.html#importing-the-api
// as to why these defines are necessary
#define PY_ARRAY_UNIQUE_SYMBOL turbodbc_numpy_API
#define NO_IMPORT_ARRAY
#include <numpy/ndarrayobject.h>

#include <Python.h>

#include <cstring>
#include <vector>
#include <sql.h>

namespace turbodbc { namespace result_sets {



namespace {

	struct masked_column {
		boost::python::object data;
		boost::python::object mask;
	};

	struct numpy_type {
		int code;
		int size;
	};

	numpy_type const numpy_int_type = {NPY_INT64, 8};
	numpy_type const numpy_bool_type = {NPY_BOOL, 1};

	boost::python::object make_numpy_array(npy_intp elements, numpy_type type)
	{
		int const flags = 0;
		int const one_dimensional = 1;
		// __extension__ needed because of some C/C++ incompatibility.
		// see issue https://github.com/numpy/numpy/issues/2539
		return boost::python::object{boost::python::handle<>(__extension__ PyArray_New(&PyArray_Type,
		                                                                               one_dimensional,
		                                                                               &elements,
		                                                                               type.code,
		                                                                               nullptr,
		                                                                               nullptr,
		                                                                               type.size,
		                                                                               flags,
		                                                                               nullptr))};
	}

	PyArrayObject * get_array_ptr(boost::python::object & object)
	{
		return reinterpret_cast<PyArrayObject *>(object.ptr());
	}

	void resize_numpy_array(boost::python::object & array, npy_intp new_size)
	{
		PyArray_Dims new_dimensions = {&new_size, 1};
		int const no_reference_check = 0;
		__extension__ PyArray_Resize(get_array_ptr(array), &new_dimensions, no_reference_check, NPY_ANYORDER);
	}

	long * get_numpy_data_pointer(boost::python::object & numpy_object)
	{
		return static_cast<long *>(PyArray_DATA(get_array_ptr(numpy_object)));
	}

	boost::python::list as_python_list(std::vector<masked_column> const & objects)
	{
		boost::python::list result;
		for (auto const & object : objects) {
			result.append(boost::python::make_tuple(object.data, object.mask));
		}
		return result;
	}
}

numpy_result_set::numpy_result_set(result_set & base) :
	base_result_(base)
{
}


boost::python::object numpy_result_set::fetch_all()
{
	std::size_t processed_rows = 0;
	std::size_t rows_in_batch = base_result_.fetch_next_batch();
	auto const n_columns = base_result_.get_column_info().size();

	std::vector<masked_column> columns;
	for (std::size_t i = 0; i != n_columns; ++i) {
		masked_column column = {make_numpy_array(rows_in_batch, numpy_int_type),
		                        make_numpy_array(rows_in_batch, numpy_bool_type)};
		columns.push_back(column);
	}

	do {
		auto const buffers = base_result_.get_buffers();

		for (std::size_t i = 0; i != n_columns; ++i) {
			resize_numpy_array(columns[i].data, processed_rows + rows_in_batch);
			std::memcpy(get_numpy_data_pointer(columns[i].data) + processed_rows,
			            buffers[i].get().data_pointer(),
			            rows_in_batch * numpy_int_type.size);

			resize_numpy_array(columns[i].mask, processed_rows + rows_in_batch);
			std::memset(reinterpret_cast<std::int8_t *>(get_numpy_data_pointer(columns[i].mask)) + processed_rows,
			            0,
			            rows_in_batch);
			auto const mask_pointer = reinterpret_cast<std::int8_t *>(get_numpy_data_pointer(columns[i].mask)) + processed_rows;
			auto const indicator_pointer = buffers[i].get().indicator_pointer();
			for (std::size_t element = 0; element != rows_in_batch; ++element) {
				if (indicator_pointer[element] == SQL_NULL_DATA) {
					mask_pointer[element] = 1;
				}
			}
		}
		processed_rows += rows_in_batch;
		rows_in_batch = base_result_.fetch_next_batch();
	} while (rows_in_batch != 0);

	return as_python_list(columns);
}



} }