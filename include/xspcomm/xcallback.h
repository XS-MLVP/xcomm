#ifndef __xspcomm_xcallback__
#define __xspcomm_xcallback__

//#include "xspcomm/xutil.h"
#include "xspcomm/_not_export.h"
#include <cstdio>
#include <cstddef>
#include <type_traits>

template <typename R, typename... Args>
class xfunction : public _xfunction_ptr<R, Args...>{
	xfunction * target = nullptr;
	bool force_callable = false;
public:
	void set_force_callable(){
		this->target = this;
		this->force_callable = true;
	}
	xfunction(){
		this->target = nullptr;
		this->func = nullptr;
	}
	virtual ~xfunction(){}
	virtual R operator() (Args... __args) {
		if(this->force_callable){
			return this->target->call(__args...);
		}
		if(*this)return this->func.operator()(__args...);
		return R();
	}

	virtual R call(Args... __args) {
		if(this->force_callable){
			printf("[**** Error ****] call cannot be called in force_callable mode!\n");
		}
		if(*this)return this->func.operator()(__args...);
		return R();
	}

	xfunction & _xfunction (const xfunction & cb) {
		if(cb.force_callable){
			this->force_callable = true;
			this->target = cb.target;
			this->func = nullptr;
		}else{
			this->force_callable = false;
			this->target = nullptr;
			this->func = cb.func;
		}
		return *this;
	}

	xfunction & operator = (xfunction & cb) {
		return this->_xfunction(cb);
	}

	xfunction & operator = (const nullptr_t & n) {
		this->func = nullptr;
		return *this;
	}

	xfunction & operator = (const xfunction & cb) {
		return this->_xfunction(cb);
	}

	virtual operator bool() const {
		if(this->force_callable) return true;
		return this->func != nullptr;
	}

	bool operator == (const nullptr_t & n) const {
		if(this->force_callable)return false;
		return this->func == nullptr;
	}

	bool operator != (const nullptr_t & n) const {
		return !this->operator==(n);
	}

	template <typename F>
	typename std::enable_if<std::is_convertible<F, std::function<R(Args...)>>::value, xfunction&>::type
	operator= (F&& f) {
		this->func = std::forward<F>(f);
		return *this;
	}
	
	template <typename F, typename = typename std::enable_if<std::is_convertible<F, std::function<R(Args...)>>::value>::type>
	xfunction(F && f){
		*this = f;
	}
};

#endif
