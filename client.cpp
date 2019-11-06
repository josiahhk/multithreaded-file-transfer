#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "FIFOreqchannel.h"
#include "RequestChannel.h"
#include "MessageQueueChannel.h"
#include "SHMChannel.h"
#include<sys/wait.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

using namespace std;


class patient_thread_args{
public:
	int total_datapnt; //the total amount we are requesting (variable n)
	int person;
	BoundedBuffer* bb;
	bool is_fmr;
	__int64_t* file_size;
	char* fm = nullptr;
	filemsg* f;
	RequestChannel* chan;
	string filename;
	pthread_mutex_t* hist_lock;
	int msg_size;
	
};

void * patient_thread_function(void *arg) //what the thread does when it is called
{	
	patient_thread_args* pa = (patient_thread_args*) arg;
	if(!(pa->is_fmr)){
		for(int i =0; i <pa->total_datapnt; i++){
			double dd = i*0.004;
			datamsg d(pa->person, dd, 1);
			pa->bb->push((char*)&d, sizeof(datamsg));
		}
	}
	
	else{
		
		__int64_t off;
		__int64_t size = *(pa->file_size);
		int len; 
		off = 0;
	
		int message_size = sizeof(filemsg)+sizeof(pa->filename)+1;
		char* fm = new char[message_size];
		filemsg f(0, 0);
		fm = (char*)&f;
		//while we never use fn, we're copying data onto fm, which we do use
		char* fn = fm+sizeof(filemsg); 
		copy((pa->filename).begin(), (pa->filename).end(), fn);
		fn[(pa->filename).size()] = '\0';

		cout<<"size: "<<size<<endl;	
		for(size; size>0; size -= len){
			if(size>pa->msg_size){ len = pa->msg_size;}
			else {len = size;}
			char* fm = new char[message_size];
			filemsg f(off, len) ;
			fm = (char*)&f;
			
			char* fn = fm+sizeof(filemsg); 
			copy((pa->filename).begin(), (pa->filename).end(), fn);
			fn[(pa->filename).size()] = '\0';
			pa->bb->push((char*)&f, message_size);

			off += len;
			
		}
	}
	cout<<"END PATIENT"<<endl;
	

}

class worker_thread_args{
public:
   BoundedBuffer* bb;
   RequestChannel* chan;
   vector<Histogram>* hist_vec;
   pthread_mutex_t* hist_lock;
   int worker_num;
   string filename;
   pthread_mutex_t* write_lock;
   pthread_cond_t* cond;
   __int64_t size;
  string ipc;
   
};


void *worker_thread_function(void *arg)
{	

	FILE* ost;
	worker_thread_args* wt = (worker_thread_args*) arg;
	cout<<"Worker num "<<wt->worker_num<<" begin"<<endl;
	MESSAGE_TYPE nc = NEWCHANNEL_MSG;
	pthread_mutex_lock(wt->hist_lock);
	wt->chan->cwrite((char*)&nc, sizeof(nc));
	char* name = wt->chan->cread();
	cout<<(string)name<<endl;
	//cout<<"worker"<<endl;

	RequestChannel* newchan;
	if(wt->ipc == "f")
		newchan = new FIFORequestChannel(name, RequestChannel::CLIENT_SIDE);
	if(wt->ipc == "q")
		newchan = new MessageQueueChannel(name, RequestChannel::CLIENT_SIDE);
	if(wt->ipc == "s")
		newchan = new SHMChannel(name, RequestChannel::CLIENT_SIDE);
	pthread_mutex_unlock(wt->hist_lock);
	while(1){
	//	cout<<"Worker num "<<wt->worker_num<<" loop"<<endl;
		vector<char> v = wt->bb->pop();
		char* c = (char*)v.data();
		MESSAGE_TYPE m = *(MESSAGE_TYPE*)c;
		if(m == QUIT_MSG){
			//cout<<"quit "<<wt->worker_num<<endl;
			MESSAGE_TYPE quit = QUIT_MSG;
			newchan->cwrite(c, sizeof(c));
			//delete newchan;
			break;
		}
		else if(m == DATA_MSG){
			datamsg* d = (datamsg*)c;
			int person = d->person;
			newchan->cwrite(c, sizeof(datamsg));
			double* data = (double*)newchan->cread();
			wt->hist_vec->at(person-1).update(*data);
		}
		else if(m == FILE_MSG){
			string file = "received/"+wt->filename;
			const char* cc = file.c_str();
			
			FILE *fp = fopen(cc, "ab+");
			//cout<<wt->size<<endl;
			fseek(fp, wt->size , SEEK_SET);
			fputc('\0', fp);
			fclose(fp);
			ost = fopen(cc, "rb+");
			
			if(ost == nullptr)
				cout<<"BROKEN!"<<endl;
			filemsg f = *(filemsg*)c;
			string filename = c + sizeof (filemsg);
			int message_size = sizeof(filename)+sizeof(filemsg)+1;
			newchan->cwrite(c, message_size);
			char* data = newchan->cread();
		
			fseek(ost, f.offset, SEEK_SET);
			fwrite(data, sizeof(char), f.length, ost);
			
			//cout<<"Worker num "<<wt->worker_num<<" postunlock"<<endl;
			fclose(ost);
		}
	
	}
	
	//cout<<"Worker num "<<wt->worker_num<<" end"<<endl;
	
}



int main(int argc, char *argv[]){
	
    int n = 100;    //default number of requests per "patient"
    int p = 10;     // number of patients [1,15]
    int w = 1;    //default number of worker threads
    int b = 20; 	// default capacity of the request buffer, you should change this default
	int m = MAX_MESSAGE;	// default capacity of the file buffer
	string ipc = "s";
	string dm = "";
	string filename = "1.csv";
    srand(time_t(NULL));
	bool mIn = false;

    
	int c = 0;
	while ((c = getopt (argc, argv, "b:n:w:p:m:f:i:")) != -1)
        switch (c){
        case 'b':
            b =  atoi(optarg);
            break;
        case 'n':
            n = atoi(optarg);
            break;
		case 'w':
            w = atoi(optarg);
            break;
		case 'p':
            p = atoi(optarg);
            break;
		case 'm':
            m = atoi(optarg);
			dm = optarg;
			mIn = true;
            break;
		case 'f':
            filename = optarg;
            break;
		case 'i':
            ipc = optarg;
            break;
		}


	if(mIn == false)
		dm = "256";

    int pid = fork();
    if (pid == 0){
		// modify this to pass along m
        execl ("dataserver", "dataserver", dm.c_str(), ipc.c_str(), (char *)NULL);
        
    }

	RequestChannel* chan;
	if(ipc == "f")
		chan = new FIFORequestChannel("control", RequestChannel::CLIENT_SIDE);
	if(ipc == "q")
		chan = new MessageQueueChannel("control", RequestChannel::CLIENT_SIDE);
	if(ipc == "s")
		chan = new SHMChannel("control", RequestChannel::CLIENT_SIDE);

    BoundedBuffer request_buffer(b);
	
	struct timeval start, end;
	gettimeofday (&start, 0);
	HistogramCollection hc;
	pthread_mutex_t hist_lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER;
	


	if(filename == ""){
		pthread_t patient_t[p];
		pthread_t worker_t[w];
		vector<Histogram> hist_vec;

		/* Start all threads here */
		patient_thread_args pargs[p];
		worker_thread_args wargs[w];
		
		
		for(int i = 0; i < p; i++){
			pargs[i].person = i+1;
			pargs[i].total_datapnt = n;
			pargs[i].bb = &request_buffer;
			pargs[i].is_fmr = 0;
			Histogram h(20, -1, 1);
			hist_vec.push_back(h);
			pthread_create(&patient_t[i], NULL, patient_thread_function, &pargs[i]);
		}


	
		for(int i = 0; i < w; i++){
			wargs[i].chan = chan;
			wargs[i].bb = &request_buffer;
			wargs[i].hist_vec = &hist_vec;
			wargs[i].hist_lock = &hist_lock;
			wargs[i].worker_num = i;
			wargs[i].ipc = ipc;
			pthread_create(&worker_t[i], NULL, worker_thread_function, &wargs[i]);
		}
		
		/* Join all threads here */
		
		for(int i = 0; i < p; i++){
			//cout<<"Quitting p: "<<i<<endl;
			pthread_join(patient_t[i], 0);
		}
	
		MESSAGE_TYPE quit = QUIT_MSG;
		for(int i = 0; i < w; i++){
			request_buffer.push((char*)&quit, sizeof(QUIT_MSG));
		}
		
		for(int i = 0; i < w; i++){
			//cout<<"Quitting w: "<<i<<endl;
			pthread_join(worker_t[i], 0);
		}
		
		gettimeofday (&end, 0);
		for(int i = 0; i < p; i++){
			Histogram* h = new Histogram(0, 0, 0);
			h = &hist_vec[i];
			hc.add(h);
		}
		hc.print ();
		
	}
	else{
		__int64_t size;
		int message_size = sizeof(filemsg)+sizeof(filename)+1;
		char* fm = new char[message_size];
		filemsg f(0, 0);
		fm = (char*)&f;
		//while we never use fn, we're copying data onto fm, which we do use
		char* fn = fm+sizeof(filemsg); 
		copy((filename).begin(), (filename).end(), fn);
		fn[(filename).size()] = '\0';
		chan->cwrite(fm, message_size); 
		size = *(__int64_t*)chan->cread();
		pthread_t worker_t[w];
		worker_thread_args wargs[w];
		pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
		
		pthread_t p;
		patient_thread_args pargs;
		pargs.bb = &request_buffer;
		pargs.hist_lock = &hist_lock;
		pargs.is_fmr = 1;
		//pargs.chan = chan;
		pargs.filename = filename;
		pargs.file_size = &size;
		pargs.msg_size = m;
		pthread_create(&p, NULL, patient_thread_function, &pargs);
		
		__int64_t file_len = 0;
		for(int i = 0; i < w; i++){
			wargs[i].chan = chan;
			wargs[i].bb = &request_buffer;
			wargs[i].hist_lock = &hist_lock;
			wargs[i].worker_num = i;
			wargs[i].size = size;
			wargs[i].filename = filename;
			wargs[i].write_lock = &write_lock;
			wargs[i].cond = &cond;
			wargs[i].ipc = ipc;
			pthread_create(&worker_t[i], NULL, worker_thread_function, &wargs[i]);
		}
		
		
		pthread_join(p, 0);
		
		MESSAGE_TYPE* quit = (MESSAGE_TYPE*)QUIT_MSG;
		for(int i = 0; i < w; i++){
			request_buffer.push((char*)&quit, sizeof(QUIT_MSG));
		}
		
	
		for(int i = 0; i < w; i++){
			//cout<<"Quitting w: "<<i<<endl;
			pthread_join(worker_t[i], 0);
		}
		
	}
	
	gettimeofday (&end, 0);
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
	
    delete chan;
    
}