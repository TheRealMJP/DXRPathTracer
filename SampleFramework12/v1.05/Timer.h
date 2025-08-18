//=================================================================================================
//
//	MJP's DX11 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "PCH.h"

namespace SampleFramework12
{

class Timer
{

public:

    Timer();
    ~Timer();

    void Update();

    int64_t ElapsedSeconds() const;
    float ElapsedSecondsF() const;
    double ElapsedSecondsD() const;
    int64_t DeltaSeconds() const;
    float DeltaSecondsF() const;
    double DeltaSecondsD() const;

    int64_t ElapsedMilliseconds() const;
    float ElapsedMillisecondsF() const;
    double ElapsedMillisecondsD() const;
    int64_t DeltaMilliseconds() const;
    float DeltaMillisecondsF() const;
    double DeltaMillisecondsD() const;

    int64_t ElapsedMicroseconds() const;
    float ElapsedMicrosecondsF() const;
    double ElapsedMicrosecondsD() const;
    int64_t DeltaMicroseconds() const;
    float DeltaMicrosecondsF() const;
    double DeltaMicrosecondsD() const;

protected:

    int64_t startTime;

    int64_t frequency;
    double frequencyD;

    int64_t elapsed;
    int64_t delta;

    float elapsedF;
    float deltaF;

    double elapsedD;
    double deltaD;

    int64_t elapsedSeconds;
    int64_t deltaSeconds;

    float elapsedSecondsF;
    float deltaSecondsF;

    double elapsedSecondsD;
    double deltaSecondsD;

    int64_t elapsedMilliseconds;
    int64_t deltaMilliseconds;

    float elapsedMillisecondsF;
    float deltaMillisecondsF;

    double elapsedMillisecondsD;
    double deltaMillisecondsD;

    int64_t elapsedMicroseconds;
    int64_t deltaMicroseconds;

    float elapsedMicrosecondsF;
    float deltaMicrosecondsF;

    double elapsedMicrosecondsD;
    double deltaMicrosecondsD;
};

}