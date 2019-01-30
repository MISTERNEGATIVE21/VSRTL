#ifndef VSRTL_SIGNAL_H
#define VSRTL_SIGNAL_H

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <type_traits>
#include <variant>

#include "vsrtl_binutils.h"
#include "vsrtl_defines.h"

// Signals cannot exist outside of components!

namespace vsrtl {

class Component;

class OutputSignalBase {
    friend class Component;

public:
    OutputSignalBase(Component* parent, const char* name) : m_name(name), m_parent(parent) {}
    virtual ~OutputSignalBase() {}

    // Checks whether a propagation function has been set for the signal - required for the signal to generate its
    // next-state value
    virtual bool hasPropagationFunction() const = 0;
    virtual void propagate() = 0;
    Component* getParent() { return m_parent; }
    virtual unsigned int width() const = 0;

    // Value access operators
    virtual explicit operator int() const = 0;
    virtual explicit operator unsigned int() const = 0;
    virtual explicit operator bool() const = 0;

    const char* getName() const { return m_name; }

protected:
    const char* m_name;

private:
    Component* m_parent = nullptr;
};

template <unsigned int bitwidth>
class OutputSignal : public OutputSignalBase {
public:
    OutputSignal(Component* parent, const char* name) : OutputSignalBase(parent, name) {}

    unsigned int width() const override { return bitwidth; }

    template <typename T = unsigned int>
    T value() {
        return static_cast<T>(*this);
    }

    std::function<std::array<bool, bitwidth>()> getFunctor() {
        return [=] { return m_value; };
    }

    void connect(OutputSignal<bitwidth>& otherOutput) { setPropagationFunction(otherOutput.getFunctor()); }

    // Casting operators
    explicit operator int() const override { return signextend<int32_t, bitwidth>(accBVec<bitwidth>(m_value)); }
    explicit operator unsigned int() const override { return accBVec<bitwidth>(m_value); }
    explicit operator bool() const override { return m_value[0]; }

    /**
     * @brief setValue
     * Used when hard-setting a signals value (ie. used by registers when resetting their output signals
     * @param v
     */
    void setValue(std::array<bool, bitwidth> v) { m_value = v; }

    void propagate() override { m_value = m_propagationFunction(); }
    bool hasPropagationFunction() const override { return m_propagationFunction != nullptr; }
    void setPropagationFunction(std::function<std::array<bool, bitwidth>()> f) { m_propagationFunction = f; }

private:
    // Binary array representing the current value of the primitive
    std::array<bool, bitwidth> m_value = buildUnsignedArr<bitwidth>(0);
    std::function<std::array<bool, bitwidth>()> m_propagationFunction;
};

using InputSignalRawPtr = OutputSignalBase***;
using OutputSignalRawPtr = OutputSignalBase*;

/**
 * Input signals can either refer to other input signals, or to an output signal
 * of other components. To represent this, we use std::variant
 */

class InputSignalBase {
public:
    InputSignalBase(Component* parent, const char* name) : m_name(name), m_parent(parent) {}
    Component* getParent() { return m_parent; }
    virtual Component* getConnectedParent() = 0;
    virtual bool isConnected() const = 0;

    /*
    Wish i could do something like

    template<typename T>
    virtual T getValue() const = 0;
    */

    virtual explicit operator int() const = 0;
    virtual explicit operator unsigned int() const = 0;
    virtual explicit operator bool() const = 0;

    const char* getName() const { return m_name; }

private:
    const char* m_name;

    Component* m_parent = nullptr;
};

template <class... Ts>
struct overloadVisitor : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloadVisitor(Ts...)->overloadVisitor<Ts...>;

template <uint32_t bitwidth>
class InputSignal : public InputSignalBase {
public:
    InputSignal(Component* parent, const char* name) : InputSignalBase(parent, name) {
        // Initially an input signal is connected to a null Signal
        m_signal = static_cast<OutputSignal<bitwidth>*>(nullptr);
    }

    explicit operator int() const override { return value<int>(); }
    explicit operator unsigned int() const override { return value<unsigned int>(); }
    explicit operator bool() const override { return value<bool>(); }

    template <typename T>
    T value() const {
        T value;
        std::visit(
            overloadVisitor{
                [&value](OutputSignal<bitwidth>* arg) { value = arg->template value<T>(); },
                [&value](InputSignal<bitwidth>* arg) { value = arg->template value<T>(); },
            },
            m_signal);
        return value;
    }

    bool isConnected() const override {
        bool connected;
        std::visit(
            overloadVisitor{
                [&connected](OutputSignal<bitwidth>* arg) { connected = arg != nullptr; },
                [&connected](InputSignal<bitwidth>* arg) { connected = arg->isConnected(); },
            },
            m_signal);
        return connected;
    }

    Component* getConnectedParent() {
        Component* connectedParent;
        std::visit(
            overloadVisitor{
                [&connectedParent](OutputSignal<bitwidth>* arg) { connectedParent = arg->getParent(); },
                [&connectedParent](InputSignal<bitwidth>* arg) { connectedParent = arg->getParent(); },
            },
            m_signal);
        return connectedParent;
    }

    void connect(InputSignal<bitwidth>& otherInput) {
        verifyIsNotConnected();
        m_signal = &otherInput;
    }

    void connect(OutputSignal<bitwidth>& output) {
        verifyIsNotConnected();
        m_signal = &output;
    }

private:
    void verifyIsNotConnected() {
        bool isConnected;
        std::visit(
            overloadVisitor{
                // Default variant value is OutputSignal. Not connected if this is a nullptr
                [&isConnected](OutputSignal<bitwidth>* arg) { isConnected = arg != nullptr; },
                // If the signal is to an InputSignal, we are sure that this input has been previously connected
                [&isConnected](InputSignal<bitwidth>*) { isConnected = true; },
            },
            m_signal);

        if (isConnected) {
            std::string err("ERROR! trying to connect an input which has already been connected.\n");
            err += "Signal: \t\t'";
            err += this->getName();
            err += "'\nOf component: \t'";
            err += getParent()->getName();
            err += "'\nIs already connected to \nSignal: \t\t'";
            std::string toThisSignal, toThisComponent;
            std::visit(
                overloadVisitor{
                    [&toThisSignal, &toThisComponent, this](OutputSignal<bitwidth>* arg) {
                        toThisSignal = arg->getName();
                        toThisComponent = getConnectedParent()->getName();
                    },
                    [&toThisSignal, &toThisComponent, this](InputSignal<bitwidth>* arg) {
                        toThisSignal = arg->getName();
                        toThisComponent = getParent()->getName();
                    },
                },
                m_signal);
            err += toThisSignal;
            err += "'\nOf component: \t'";
            err += toThisComponent;
            err += "'\n";
            std::cout << err << std::endl;
            assert(false);
        }
    }
    std::variant<OutputSignal<bitwidth>*, InputSignal<bitwidth>*> m_signal;
};

/*
template <uint32_t width>
class OutputSignal {

private:
    Signal<width> m_out;
    std::string m_name;

}

*/

/**
 *  Connects the following pattern:
 * IN   OUT IN  OUT
 *  _____    ____
 * |    |   |    |
 * |    ->-->    |
 * |____|   |____|
 *
 */
template <unsigned int bitwidth>
void operator>>(OutputSignal<bitwidth>& fromThisOutput, InputSignal<bitwidth>& toThisInput) {
    toThisInput.connect(fromThisOutput);
}

/**
 *  Connects the following pattern:
 * IN   IN   OUT  OUT
 *   _____________
 *  |    _____   |
 *  |   |    |   |
 *  ->-->    |   |
 *  |   |____|   |
 *  |____________|
 *
 */
template <unsigned int bitwidth>
void operator>>(InputSignal<bitwidth>& fromThisInput, InputSignal<bitwidth>& toThisInput) {
    toThisInput.connect(fromThisInput);
}

/**
 *  Connects the following pattern:
 * IN   IN   OUT  OUT
 *   _____________
 *  |    _____   |
 *  |   |    |   |
 *  |   |   ->--->
 *  |   |____|   |
 *  |____________|
 *
 */
template <unsigned int bitwidth>
void operator>>(OutputSignal<bitwidth>& fromThisOutput, OutputSignal<bitwidth>& toThisOutput) {
    toThisOutput.connect(fromThisOutput);
}

}  // namespace vsrtl

#endif  // VSRTL_SIGNAL_H
