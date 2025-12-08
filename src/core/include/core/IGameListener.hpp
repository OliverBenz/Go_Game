#pragma once

namespace go {

class IGameListener {
public:
    virtual ~IGameListener() = default;
    virtual void onBoardChange() = 0;
};

}