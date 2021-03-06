////////////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016-2018 Le Hoang Quyen
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//		http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <sys/time.h>
#include <mutex>
#include <sstream>
#include <assert.h>

#include "../Timer.h"

namespace HQRemote {
	static std::mutex g_lock;
	
	struct Time : public time_checkpoint_t {
		Time()
		{
			clock_gettime(CLOCK_MONOTONIC , this);
		}
	};
	
	static inline Time& getStartTime() {
		static Time start;
		return start;
	}
	
	static inline uint64_t _convertToTimeCheckPoint64(const time_checkpoint_t& time){
		return (uint64_t)time.tv_sec * 1000000000ull + time.tv_nsec;
	}
	
	static inline void _convertToTimeCheckPoint(time_checkpoint_t& checkPoint, uint64_t time64) {
		checkPoint.tv_sec = (time_t) (time64 / 1000000000ull);
		checkPoint.tv_nsec = (long)(time64 % 1000000000ull);
	}
	
	///get time check point
	void getTimeCheckPoint(time_checkpoint_t& checkPoint) {
		auto & startTime = getStartTime();
		
		clock_gettime(CLOCK_MONOTONIC , &checkPoint);
		
		//avoid time overflow
		checkPoint.tv_sec -= startTime.tv_sec;
		checkPoint.tv_nsec -= startTime.tv_nsec;
	}
	
	///get elapsed time in seconds between two check points
	double getElapsedTime(const time_checkpoint_t& point1 , const time_checkpoint_t& point2) {
		time_t sec = point2.tv_sec - point1.tv_sec;
		long nsec = point2.tv_nsec - point1.tv_nsec;
		
		return ((double)sec + (double)nsec / 1e9);
	}
	
	double getElapsedTime64(uint64_t point1, uint64_t point2) {
		time_checkpoint_t t1, t2;
		
		_convertToTimeCheckPoint(t1, point1);
		_convertToTimeCheckPoint(t2, point2);

#ifdef DEBUG
		auto point1_test = convertToTimeCheckPoint64(t1);
		auto point2_test = convertToTimeCheckPoint64(t2);
		assert(point1 == point1_test && point2 == point2_test);
#endif
		
		return getElapsedTime(t1, t2);
	}
	
	uint64_t getTimeCheckPoint64() {
		time_checkpoint_t time;
		getTimeCheckPoint(time);
		auto re = _convertToTimeCheckPoint64(time);

		return re;
	}
	
	uint64_t convertToTimeCheckPoint64(const time_checkpoint_t& time){
		return _convertToTimeCheckPoint64(time);
	}
	
	void convertToTimeCheckPoint(time_checkpoint_t& checkPoint, uint64_t time64) {
		_convertToTimeCheckPoint(checkPoint, time64);
	}
	
	CString getCurrentTimeStr() {
		std::lock_guard<std::mutex> lg(g_lock);
		
		timeval tv;
		tm* ptm;
		gettimeofday(&tv, NULL);
		ptm = localtime(&tv.tv_sec);
		
		std::stringstream ss;
		ss << ptm->tm_mday << "-" << ptm->tm_mon + 1 << "-" << ptm->tm_year + 1900 << "-"
		<< ptm->tm_hour << "-" << ptm->tm_min << "-" << ptm->tm_sec << "-" << tv.tv_usec;
		
		auto str = ss.str();
		return CString(str.c_str(), str.size());
	}
	
	uint64_t generateIDFromTime(const time_checkpoint_t& time) {
		return _convertToTimeCheckPoint64(time);
	}
}
