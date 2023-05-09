#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

// struct for storing data-packet queue
typedef struct DataQue{
    long sn; // sequence number
    double gentm, timeout; // generation(arrival) time of a packet & timeout time
    struct DataQue*link;
    int ack;
} DataQue;

// struct for storing ACK Queue
typedef struct AckQue{
    long sn;
    double ack_rtm; // reception time of an ACK at a sender's perspective
    struct AckQue* link;
} AckQue;

DataQue *WQ_front, *WQ_rear;
DataQue *TransitQ_front, *TransitQ_rear;
AckQue *AQ_front, *AQ_rear;

long seq_n=0;
long transit_pknum=0; // regarding W of sliding window, indicates how many packets are in TransitQ
long next_acksn=0;
double cur_tm, next_pk_gentm; // current time and the time of next packet generation
long t_pknum=0; // accumulated success num of transmission
double t_delay=0; // accumulated delay time

long N; // packets transmitted
double timeout_len;
int W;
float a, t_pk, t_pro; // a for (t_pro/t_pk)
float lambda, p, p_ack; // lambda for poisson distribution of packet arrival, p for transmission error rate, p_ack for ack error rate

float random_p(void);
void pk_gen(double);
void suc_transmission(long);
void re_transmit(void);
int transmit_pk(void);
void receive_pk(long, double);
void enque_Ack(long);
void cur_tm_update(void);
void print_performance_measure(void);


float random_p(void){
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srand((unsigned int)ts.tv_nsec);
    float p= (float)rand()/(float)RAND_MAX;
    return p;
}

void pk_gen(double tm){
    DataQue *ptr;

    ptr=malloc(sizeof(DataQue));
    ptr->sn=seq_n;
    ptr->gentm=tm;
    ptr->link=NULL;
    ptr->ack=0;
    seq_n++; // not using circular sn just to simplify

    if(WQ_front==NULL)
        WQ_front=ptr;
    else WQ_rear->link=ptr;
    WQ_rear=ptr;
}

// [No error] For the first k ACKs in the queue(already received ones), 
// their seq numbers must match with the seq numbers of the first k packets in the Transit queue.
// [Otherwise] It means that some packets in the transit queue cannot be deleted from the queue.
// They should be retransmitted later. In this case, the ACK for that sn just evaporates... 
// And the front pointer is moved forward.
void suc_transmission(long sn){
    DataQue *ptr;
    DataQue *tmp;
    AckQue *aptr;

    ptr=TransitQ_front;
    while(ptr){
        if(ptr==TransitQ_front){
            if(TransitQ_front->ack==1){ 
                TransitQ_front=TransitQ_front->link;
                if(TransitQ_front==NULL)
                    TransitQ_rear=NULL;
                free(ptr);
                transit_pknum--;
                break;
            }
            if(TransitQ_front->sn==sn){// ACK might be bigger than expected, when smaller ACK was lost.
                ptr=TransitQ_front;
                TransitQ_front=TransitQ_front->link;
                if(TransitQ_front==NULL)
                    TransitQ_rear=NULL;
                free(ptr);
                transit_pknum--;
                break;  
            } 
        }
        else if(ptr->sn==sn && ptr->ack==0){
            ptr->ack=1;
            // tmp=ptr->link->link;
            // free(ptr->link);
            // ptr->link=tmp;
            // transit_pknum--; 
            // transit_pknum still indicates the number of open windows, even if the 
            // packet has been deleted from TransitQ.
            break;
        }
        ptr=ptr->link;
    }
    aptr=AQ_front;
    AQ_front=aptr->link;
    if(AQ_front==NULL) AQ_rear=NULL;
    free(aptr);
}

// DataQue* re_transmit(DataQue *ptr){
//     ptr->link=WQ_front;
//     if(WQ_front==NULL)
//         WQ_rear=ptr;
//     WQ_front=ptr;
//     return ptr->link;
// }


void re_transmit(void){
    DataQue *temp=TransitQ_front->link;
    TransitQ_front->link=WQ_front;
    TransitQ_front->ack=-1; // flag for telling that this packet has failed before
    if(WQ_front==NULL)
        WQ_rear=TransitQ_front;
    WQ_front=TransitQ_front;
    TransitQ_front=temp;
    if(TransitQ_front==NULL) TransitQ_rear=NULL;
    transit_pknum--;
}

// assuming cur_tm is the starting point of the transmission,
// update time, delete the first packet from the working queue,
// and enqueue it to the Transit queue.
int transmit_pk(void){
    DataQue *ptr;

    cur_tm+=t_pk; // time update(1)
    WQ_front->timeout=cur_tm+timeout_len;
    ptr=WQ_front;
    WQ_front=WQ_front->link;
    if(WQ_front==NULL) WQ_rear=NULL;
    if(ptr->ack==-1){ // retransmitting
        if(TransitQ_front==NULL){
            TransitQ_rear=ptr;
            TransitQ_rear->link=NULL;
            TransitQ_front=ptr;
        }
        else{
            ptr->link = TransitQ_front->link;
            TransitQ_front=ptr;
        }
        transit_pknum++;
        return -1;
    }
    else if(ptr->ack==0){
        if(TransitQ_front==NULL)
            TransitQ_front=ptr;
        else TransitQ_rear->link=ptr;
        ptr->link=NULL;
        TransitQ_rear=ptr;
        transit_pknum++;
        return 1;
    }
    else{
        fprintf(stderr, "wrong packet has been retransmitted\n");
        exit(1);
    }

}
int count=0;
// receiver's role!
// if transmission error happens or next_acksn!=seqn, 
// receiver doesn't send the ACK for the packet.
void receive_pk(long seqn, double gtm){
    // **important** // shouldn't ACK next packets before ACKing a certain packet.
    // however, ACK may be lost, so receiver should ACK already ACKed packets.
    if(next_acksn>=seqn){ 
        if(next_acksn==seqn){
            t_delay+=cur_tm+t_pro-gtm; // receive time-generate time
            t_pknum++;
        }
        next_acksn++;
        enque_Ack(seqn);
    }
}

void enque_Ack(long seqn){
    AckQue *aptr;
    if(random()>p_ack){ // ack success
        aptr=malloc(sizeof(AckQue));
        aptr->sn=seqn;
        aptr->ack_rtm = cur_tm+2*t_pro;
        aptr->link=NULL;

        if(AQ_front==NULL)
            AQ_front=aptr;
        else AQ_rear->link=aptr;
        AQ_rear=aptr;
    }
}

// time update(2)
void cur_tm_update(void){
    double tm;

    if ((WQ_front!=NULL) && (transit_pknum<W)) return; // because cur_tm should be both the endpoint of transmission and the starting point of transmission
    // move time to the closest event among(ACK/timeout/packet generation)
    else{
        if(AQ_front==NULL)
            tm=next_pk_gentm;
        else if(AQ_front->ack_rtm<next_pk_gentm)
            tm=AQ_front->ack_rtm;
        else tm=next_pk_gentm;

        if(TransitQ_front!=NULL){
            if(TransitQ_front->timeout<tm)
                tm=TransitQ_front->timeout;
        }
        if(tm>cur_tm) cur_tm=tm;
    }
}

void print_performance_measure(void){
    double util;
    double m_delay;

    m_delay=t_delay/t_pknum;
    util=(t_pknum*t_pk)/cur_tm;

    printf("Utilization: %f, Delay per packet: %f\n", util, m_delay);
    printf("Total simulation time: %f, Total packet transmission: %ld\n", cur_tm, N);
    printf("Window: %d, Timeout_len: %f, Transmission time: %f, Propagation time: %f\n", W, timeout_len, t_pk, t_pro);
    printf("Last generated packet: %ld\n", seq_n-1);
    printf("Transit Queue Leftovers: ");
    while(TransitQ_front!=NULL){
        printf("%ld ", TransitQ_front->sn);
        TransitQ_front=TransitQ_front->link;
    }
    printf("\n");
    printf("WQ Leftovers: ");
    while(WQ_front!=NULL){
        printf("%ld ", WQ_front->sn);
        WQ_front=WQ_front->link;
    }
    printf("\n");

}


int main(void){
    // input parameter setting
    N=10000;
    timeout_len=6;
    t_pk = 0.5;
    t_pro = 3;
    a =(t_pro/t_pk);
    W = (int)(2*a+1)-3; // (2*a+1)-3 for low load, (2*a+1) for medium load, (2*a+1)+3 for heavy load
    lambda=0.1;
    p= 0.05;
    p_ack = 0.01;

    // initialize
    WQ_front=WQ_rear=NULL;
    TransitQ_front=TransitQ_rear=NULL;
    AQ_front=AQ_rear=NULL;
    cur_tm = -log(random_p())/lambda;
    pk_gen(cur_tm);
    next_pk_gentm = cur_tm-log(random_p())/lambda;

    // simulation loop
    while(t_pknum<N){
        // dealing with ACKs received
        while(AQ_front!=NULL){
            if(AQ_front->ack_rtm<=cur_tm)
                suc_transmission(AQ_front->sn);
            else break;
        }

        // timeout!
        // first k packets in TransitQ whose timeout is smaller than(or same) cur_tm
        // should be retransmitted. 
        if(TransitQ_front!=NULL){
            if(TransitQ_front->timeout<=cur_tm){
                if(TransitQ_front->ack>0){
                    fprintf(stderr, "acked packet hasn't been removed from TransitQ.\n");
                    exit(1);
                }
                re_transmit();
            }
        }

        // New packet arrival checking
        while(next_pk_gentm<=cur_tm){
            pk_gen(next_pk_gentm);
            next_pk_gentm+=-log(random_p())/lambda;
        }

        // transmit the first packet in the working queue
        // only if the window is still open.
        if((WQ_front!=NULL)&&(transit_pknum<W)){
            int res = transmit_pk(); 
            if(res==1){
                receive_pk(TransitQ_rear->sn, TransitQ_rear->gentm);
            }
            else {
                receive_pk(TransitQ_front->sn, TransitQ_front->gentm);
            }
        }

        cur_tm_update();

    }
    print_performance_measure();
    return 0;
}

