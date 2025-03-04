// Copyright 2018-2024 Alfredo A. Correa

#ifndef MULTI_ADAPTORS_TOTALVIEW_HPP
#define MULTI_ADAPTORS_TOTALVIEW_HPP

#include <cassert>
#include <cstdarg>  // TODO remove
#include <cstdio>

#include <array>
#include <complex>

#include "../adaptors/../array.hpp"

#include "../src/tv_data_display.c"  // you have to find the directory with the totalview include files
#include "tv_data_display.h"  // you have to find the directory with the totalview include files
// ^^^^^^^^^^^ this can produce problemas later with linking
// https://docs.roguewave.com/totalview/2018.1/html/index.html#page/Reference_Guide%2FCompilingAndLinkingTV_data_display.html%23ww1738654

template<typename T> constexpr char const* pretty_name = "unknown";

template<> constexpr char const* pretty_name<double> = "double";
template<> constexpr char const* pretty_name<float>  = "float";

template<> constexpr char const* pretty_name<std::complex<double>> = "std::complex<double>";
template<> constexpr char const* pretty_name<std::complex<float>>  = "std::complex<float>";

template<> constexpr char const* pretty_name<long> = "long";
template<> constexpr char const* pretty_name<int>  = "int";

template<class TT>
#ifdef __GCC__
__attribute__((used))
#endif
int
TV_ttf_display_type(boost::multi::array<TT, 1> const* mad1P) {
	if(not mad1P->is_empty()) {
		std::array<char, 128> tname;  // char tname[128];
		snprintf(tname.data(), tname.size(), "%s[%ld]", pretty_name<TT>, (long)mad1P->size());  //, (long)mad1P->stride());
		int result = TV_ttf_add_row("elements", tname.data(), mad1P->origin());
		if(result != 0) {
			int res = fprintf(stderr, "TV_ttf_add_row returned error %d\n", result);
			assert(res > -1);
			return TV_ttf_format_failed;
		}
	}
	return TV_ttf_format_ok_elide;
}

template<class TT>
#ifdef __GCC__
__attribute__((used))
#endif
int
TV_ttf_display_type(boost::multi::array<TT, 2> const* mad2P) {
	if(not mad2P->is_empty()) {
		std::arra<char, 128> tname;  // char tname[128];
		using std::get;
		snprintf(tname.data(), tname.size(), "%s[%ld][%ld]", pretty_name<TT>, (long)get<0>(mad2P->sizes()), (long)get<1>(mad2P->sizes()));  //, (long)mad1P->stride());
		int result = TV_ttf_add_row("elements", tname.data(), mad2P->origin());

		if(result != 0) {
			int res = fprintf(stderr, "TV_ttf_add_row returned error %d\n", result);
			assert(res >= 0);
			return TV_ttf_format_failed;
		}
	}
	return TV_ttf_format_ok_elide;
}

template<class TT>
#ifdef __GCC__
__attribute__((used))
#endif
int
TV_ttf_display_type(boost::multi::subarray<TT, 1> const* mad2P) {
	boost::multi::array<TT, 1> const value = *mad2P;
	return TV_ttf_display_type(std::addressof(value));
}

template<class TT>
#ifdef __GCC__
__attribute__((used))
#endif
int
TV_ttf_display_type(boost::multi::subarray<TT, 2> const* mad2P) {
	boost::multi::array<TT, 2> const value = *mad2P;
	return TV_ttf_display_type(std::addressof(value));
}

template int TV_ttf_display_type<double>(boost::multi::array<double, 1> const*);
template int TV_ttf_display_type<float>(boost::multi::array<float, 1> const*);
template int TV_ttf_display_type<std::complex<double>>(boost::multi::array<std::complex<double>, 1> const*);
template int TV_ttf_display_type<std::complex<float>>(boost::multi::array<std::complex<float>, 1> const*);
template int TV_ttf_display_type<int>(boost::multi::array<int, 1> const*);
template int TV_ttf_display_type<long>(boost::multi::array<long, 1> const*);

template int TV_ttf_display_type<double>(boost::multi::array<double, 2> const*);
template int TV_ttf_display_type<float>(boost::multi::array<float, 2> const*);
template int TV_ttf_display_type<std::complex<double>>(boost::multi::array<std::complex<double>, 2> const*);
template int TV_ttf_display_type<std::complex<float>>(boost::multi::array<std::complex<float>, 2> const*);
template int TV_ttf_display_type<int>(boost::multi::array<int, 2> const*);
template int TV_ttf_display_type<long>(boost::multi::array<long, 2> const*);

template int TV_ttf_display_type<double>(boost::multi::subarray<double, 1> const*);
template int TV_ttf_display_type<float>(boost::multi::subarray<float, 1> const*);
template int TV_ttf_display_type<std::complex<double>>(boost::multi::subarray<std::complex<double>, 1> const*);
template int TV_ttf_display_type<std::complex<float>>(boost::multi::subarray<std::complex<float>, 1> const*);
template int TV_ttf_display_type<int>(boost::multi::subarray<int, 1> const*);
template int TV_ttf_display_type<long>(boost::multi::subarray<long, 1> const*);

template int TV_ttf_display_type<double>(boost::multi::subarray<double, 2> const*);
template int TV_ttf_display_type<float>(boost::multi::subarray<float, 2> const*);
template int TV_ttf_display_type<std::complex<double>>(boost::multi::subarray<std::complex<double>, 2> const*);
template int TV_ttf_display_type<std::complex<float>>(boost::multi::subarray<std::complex<float>, 2> const*);
template int TV_ttf_display_type<int>(boost::multi::subarray<int, 2> const*);
template int TV_ttf_display_type<long>(boost::multi::subarray<long, 2> const*);

#if defined(__INCLUDE_LEVEL__) and (not __INCLUDE_LEVEL__)

#define BOOST_TEST_MODULE "C++ Unit Tests for Multi TotalView adaptor"
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "../array.hpp"
#include "../utility.hpp"

#include <algorithm>  // transform
#include <complex>
#include <iostream>
#include <numeric>  // iota

namespace multi = boost::multi;

BOOST_AUTO_TEST_CASE(multi_1d) {

	std::vector<int> V = {10, 20, 30};

	multi::array<double, 1> const A     = {1.0, 2.0, 3.0, 4.0, 5.0};
	auto&&                        Apart = A({1, 3});

	multi::array<double, 2> const B = {
		{1.0, 2.0, 3.0},
		{4.0, 5.0, 6.0},
	};

	double sum = 0.0;
	for(auto i : A.extension()) {
		sum += A[i];
	}

	BOOST_REQUIRE( sum == 15.0 );
	BOOST_REQUIRE( B[1][0] == 4.0 );
}

#endif
#endif
