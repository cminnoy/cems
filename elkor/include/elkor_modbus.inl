/* Copyright 2012-2026 Chris Minnoy */

#ifndef ELKOR_MODBUS_HPP
#   error "do not directly include this file. Use elkor.hpp instead!"
#else

//
// ElkorModbusInterface
// ====================

inline WattsOnModbusInterface * ElkorModbusInterface::GetFirstDevice() const {
    return devices_;
}

inline WattsOnModbusInterface * ElkorModbusInterface::GetNextDevice(WattsOnModbusInterface * p) const {
    return p->next_;
}


//
// WattsOnModBusInterface
// ======================

inline std::string const & WattsOnModbusInterface::Port() const {
	return port_;
}

inline int WattsOnModbusInterface::SlaveNumber() const {
    return slave_number_;
}

inline int WattsOnModbusInterface::BaudRate() const {
    return baudrate_;
}

inline float WattsOnModbusInterface::PTRatio() const {
    return pt_ratio_;
}

inline float WattsOnModbusInterface::CTRatio() const {
    return ct_ratio_;
}

#endif /* ELKOR_MODBUS_HPP */
