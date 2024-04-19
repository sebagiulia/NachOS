#ifndef NACHOS_THREADS_CHANNEL__HH
#define NACHOS_THREADS_CHANNEL__HH

class Condition;
class Lock;
#include "condition.hh"


class Channel {
public:

    Channel(const char *debugNam);

    ~Channel();

    const char *GetName() const;
    
    //Recibir un mensaje del canal, es bloqueante.
    void Receive(int *message);

    //Enviar un mensaje al canal, el thread quedará bloqueado hasta que otro
    //thread ejecute un send.
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
