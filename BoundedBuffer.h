#ifndef BoundedBuffer_
#define BoundedBuffer_

#include <stdio.h>
#include <queue>
#include <string>
#include <pthread.h>
#include <vector>
#include "common.h"

using namespace std;


class BoundedBuffer
{

private:
	int cap;
	queue<vector<char>> q;
  
public:
	pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	pthread_cond_t cond2 =  PTHREAD_COND_INITIALIZER;
	
	
	BoundedBuffer(int _cap): cap(_cap)
	{}
	
	~BoundedBuffer(){
		
	}
	
	void push(char* data, int len){
		pthread_mutex_lock(&mtx);
		while(q.size() >= cap)
			pthread_cond_wait(&cond2, &mtx);
		vector<char> v(data, data+len);
		q.push(v);
		
		char* c = (char*)v.data();

		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mtx);
		
		//cout<<"q size "<<q.size()<<endl;
		
	}



	vector<char> pop(){
		
		pthread_mutex_lock(&mtx);
		while(q.size() <= 0){
			pthread_cond_wait(&cond, &mtx);
		}
		vector<char> v = q.front();
		q.pop();
		char* c = (char*)v.data();
		pthread_cond_signal(&cond2);
		pthread_mutex_unlock(&mtx);
		return v;
		
	}
	
};

#endif /* BoundedBuffer_ */