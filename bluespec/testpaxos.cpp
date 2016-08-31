#include <errno.h>
#include <stdio.h>
#include "PaxosIndication.h"
#include "PaxosRequest.h"
#include "GeneratedTypes.h"

static PaxosRequestProxy *echoRequestProxy = NULL;
static sem_t mutex;

class PaxosIndication : public PaxosIndicationWrapper
{
public:
	virtual void heard(uint32_t v) {
		printf("heard an echo: %d\n", v);
		sem_post(&mutex);
	}
	PaxosIndication(unsigned int id) : PaxosIndicationWrapper(id) {}
};

static void send1Amessage(uint16_t v)
{
	printf("[%s:%d] %d\n", __FUNCTION__, __LINE__, v);
	echoRequestProxy->handle1A(v);
	sem_wait(&mutex);
}

static void send1Bmessage(uint16_t b, uint16_t vb, uint32_t v, uint16_t a)
{
	printf("[%s:%d] %d\n", __FUNCTION__, __LINE__, b);
	echoRequestProxy->handle1B(b, vb, v, a);
	sem_wait(&mutex);
}

static void send2Bmessage(uint16_t b, uint32_t v, uint16_t a)
{
	printf("[%s:%d] %d\n", __FUNCTION__, __LINE__, b);
	echoRequestProxy->handle2B(b, v, a);
	sem_wait(&mutex);
}

int main(int argc, char *argv[])
{
	long actualFrequency = 0;
	long requestedFrequency = 1e9 / MainClockPeriod;

	PaxosIndication indication(IfcNames_PaxosIndicationH2S);
	echoRequestProxy = new PaxosRequestProxy(IfcNames_PaxosRequestS2H);

	int status = setClockFrequency(0, requestedFrequency, &actualFrequency);
	fprintf(stderr, "Requested main clock frequency %5.2f, actual clock frequency \
					%5.2f  MHz. status=%d, errno=%d\n",
			(double)requestedFrequency * 1.0e-6,
			(double)actualFrequency * 1.0e-6,
			status, errno);

	uint32_t ballot  = 1;
	send1Amessage(ballot);
	send1Bmessage(ballot, 0, 0, 1);
	send1Bmessage(ballot, 0, 0, 2);
	send2Bmessage(ballot, 42, 0);
	send2Bmessage(ballot, 42, 0);
	send2Bmessage(ballot, 42, 1);

	return 0;
}
