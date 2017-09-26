#ifndef BLOCK_EXEC_H
#define BLOCK_EXEC_H

#include "mbed.h"

/* Helper class to execute something whenever entering/leaving a basic block */
class BlockExecuter {
public:
    BlockExecuter(Callback<void()> exit_cb, Callback<void()> enter_cb = Callback<void()>()) :
        _exit_cb(exit_cb) {
        if((bool)enter_cb) enter_cb();
    }

    ~BlockExecuter(void) {
        _exit_cb();
    }

private:
    Callback<void()> _exit_cb;
};

#endif  //BLOCK_EXEC_H
