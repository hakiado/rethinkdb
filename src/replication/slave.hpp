#ifndef __REPLICATION_SLAVE_HPP__
#define __REPLICATION_SLAVE_HPP__

#include <queue>

#include "replication/protocol.hpp"
#include "replication/queueing_store.hpp"
#include "server/cmd_args.hpp"
#include "server/control.hpp"
#include "store.hpp"
#include "failover.hpp"

// The initial time we wait to reconnect to the master, upon failure.  In ms.
#define INITIAL_TIMEOUT  (100)

// Every failed reconnect, the timeout increases by this factor.
#define TIMEOUT_GROWTH_FACTOR 2

// But it can't surpass the cap.
#define TIMEOUT_CAP (1000*60*2)

/* if we mave more than MAX_RECONNECTS_PER_N_SECONDS in N_SECONDS then we give
 * up on the master server for a longer time (possibly until the user tells us
 * to stop) */
#define N_SECONDS (5*60)
#define MAX_RECONNECTS_PER_N_SECONDS (5)

/* This is a hack and we shouldn't be tied to this particular type */
struct btree_key_value_store_t;

namespace replication {

class slave_t :
    public home_thread_mixin_t
{
public:
    friend void run(slave_t *);

    slave_t(btree_key_value_store_t *, replication_config_t, failover_config_t);
    ~slave_t();

    /* failover module which is alerted by an on_failure() call when we go out
     * of contact with the master */
    failover_t failover;

private:
    friend class failover_t;

    /* failover callback interface */
    void on_failure();
    void on_resume();


    /* structure to tell us when to give up on the master */
    class give_up_t {
    public:
        void on_reconnect();
        bool give_up();
        void reset();
    private:
        void limit_to(unsigned int limit);
        std::queue<float> successful_reconnects;
    };

    /* Failover controllers */

    /* Control to  allow the failover state to be reset during run time */
    std::string failover_reset();

    class failover_reset_control_t : public control_t {
    public:
        failover_reset_control_t(std::string key, slave_t *slave)
            : control_t(key, "Reset the failover module to the state at startup (will force a reconnection to the master)."), slave(slave)
        {}
        std::string call(int argc, char **argv);
    private:
        slave_t *slave;
    };

    /* Control to allow the master to be changed during run time */
    std::string new_master(int argc, char **argv);

    class new_master_control_t : public control_t {
    public:
        new_master_control_t(std::string key, slave_t *slave)
            : control_t(key, "Set a new master for replication (the slave will disconnect and immediately reconnect to the new server). Syntax: \"rdb new_master host port\""), slave(slave)
    {}
        std::string call(int argc, char **argv);
    private:
        slave_t *slave;
    };

    // This is too complicated.

    give_up_t give_up_;

    /* Other failover callbacks */
    failover_script_callback_t failover_script_;

    /* state for failover */
    bool respond_to_queries_; /* are we responding to queries */
    long timeout_; /* ms to wait before trying to reconnect */

    failover_reset_control_t failover_reset_control_;

    new_master_control_t new_master_control_;

    btree_key_value_store_t *internal_store_;
    replication_config_t replication_config_;
    failover_config_t failover_config_;

    /* shutting_down_ points to a local variable within the run() coroutine; the
    destructor sets *shutting_down_ to true and pulse pulse_to_interrupt_run_loop_
    to shut down the slave. */
    bool *shutting_down_;

    /* pulse_to_interrupt_run_loop_ holds a pointer to whatever multicond_t the run
    loop is blocking on at the moment. */
    multicond_weak_ptr_t pulse_to_interrupt_run_loop_;
};

void run(slave_t *); //TODO make this static and private

}  // namespace replication

#endif  // __REPLICATION_SLAVE_HPP__
