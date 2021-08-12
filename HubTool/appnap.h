#ifndef APPNAP_H
#define APPNAP_H

class AppNapSuspenderPrivate;

class AppNapSuspender {
public:
    AppNapSuspender();
    ~AppNapSuspender();

    void suspend();
    void resume();
private:
    AppNapSuspenderPrivate *p;
};




#endif // APPNAP_H
