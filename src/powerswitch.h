#ifndef POLSWITCH_H
#define POLSWITCH_H

class PowerSwitch
{
public:
    /** Power switch states. */
    typedef enum {
        off = 0,
        direct = 1,
        cross = 2,
    } state_t;

    PowerSwitch();
    ~PowerSwitch();

    /** Open LPT port connected to switch.

      @return true on success, false otherwise. */
    bool open(const char *par_port);
    /** Set switch polarity. */
    bool setPolarity(state_t state);


private:
	int fd;
};

#endif // POLSWITCH_H
