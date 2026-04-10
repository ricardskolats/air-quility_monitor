#ifndef NETWORK_HPP
#define NETWORK_HPP

// LTE-M network controller
class cNetwork {
public:
    int  init(void);
    int  connect(void);
    void disconnect(void);
    
    bool is_connected(void) const { return connected_; }

private:
    bool connected_ = false;
};

extern cNetwork g_network;

#endif // NETWORK_HPP
