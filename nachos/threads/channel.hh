#ifndef NACHOS_THREADS_CHANNEL__HH
#define NACHOS_THREADS_CHANNEL__HH

#include "condition.hh"

class Channel {
public:

    /// Constructor: indicate which lock the condition variable belongs to.
    Channel(const char *debugNam);

    ~Channel();

    const char *GetName() const;

    void Receive(int *message);
    void Send(int message);

private:

    const char *name;
    Lock *l;
    Condition *cond_read, *cond_write;
    int msj;
    int concurrido; //cantidad de receptores esperando
    bool waiting; //esperando a que se lea un msj que ya se envio
};


#endif
