/* Copyright 2012-2025 Chris Minnoy */

#ifndef ELKOR_MODBUS_HPP
#   error "do not directly include this file. Use elkor.hpp instead!"
#else

//
// ElkorModbusInterface
// ====================

inline WattsOnModbusInterface * ElkorModbusInterface::GetFirstDevice() const {
    return devices_;
} // func

inline WattsOnModbusInterface * ElkorModbusInterface::GetNextDevice(WattsOnModbusInterface * p) const {
    return p->next_;
} // func


//
// WattsOnModBusInterface
// ======================

inline const std::string & WattsOnModbusInterface::Port() const {
	return port_;
} // func

inline int WattsOnModbusInterface::SlaveNumber() const {
    return slave_number_;
} // func

inline int WattsOnModbusInterface::BaudRate() const {
    return baudrate_;
} // func

inline float WattsOnModbusInterface::PTRatio() const {
    return pt_ratio_;
} // func

inline float WattsOnModbusInterface::CTRatio() const {
    return ct_ratio_;
} // func

#endif /* ELKOR_MODBUS_HPP */
