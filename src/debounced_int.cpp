/*
MIT License

Copyright (c) 2021 Haoran Wang.
Copyright (c) 2020 Steffen S.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <debounced_int.h>

DebouncedInt::DebouncedInt(PinName pin, PinMode mode) : input(pin, mode) {}

void DebouncedInt::rise(Callback<void()> handler)
{
    riseHandler = handler;

    if (handler)
    {
        input.rise(callback(this, &DebouncedInt::timedRise));
    }
    else
    {
        // reset input rise handler and timeout
        input.rise(nullptr);
        debounceRiseTimeout.detach();
    }
}

void DebouncedInt::timedRise()
{
    // start debounce timeout
    debounceRiseTimeout.attach(callback(this, &DebouncedInt::riseCheck), WAIT_TIME);
}

void DebouncedInt::riseCheck()
{
    if (input.read() && riseHandler)
    {
        // if button is high and handler not null
        riseHandler();
    }
}

void DebouncedInt::fall(Callback<void()> handler)
{
    fallHandler = handler;

    if (handler)
    {
        input.fall(callback(this, &DebouncedInt::timedFall));
    }
    else
    {
        // reset input fall handler and timeout
        input.rise(nullptr);
        debounceFallTimeout.detach();
    }
}

void DebouncedInt::timedFall()
{
    // start debounce timeout
    debounceFallTimeout.attach(callback(this, &DebouncedInt::fallCheck), WAIT_TIME);
}

void DebouncedInt::fallCheck()
{
    if (!input.read() && fallHandler)
    {
        // if button is low and handler not null
        fallHandler();
    }
}