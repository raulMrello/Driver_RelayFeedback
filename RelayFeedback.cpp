/*
 * RelayFeedback.cpp
 *
 *  Created on: 16 Feb 2018
 *      Author: raulMrello
 */

#include "RelayFeedback.h"



//------------------------------------------------------------------------------------
//--- PRIVATE TYPES ------------------------------------------------------------------
//------------------------------------------------------------------------------------
/** Macro para imprimir trazas de depuración, siempre que se haya configurado un objeto
 *	Logger válido (ej: _debug)
 */
static const char* _MODULE_ = "[RlyFdbk].......";
#define _EXPR_	(_defdbg && !IS_ISR())



//------------------------------------------------------------------------------------
//-- PUBLIC METHODS IMPLEMENTATION ---------------------------------------------------
//------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------
RelayFeedback::RelayFeedback(PinName fdb, LogicLevel level, PinMode mode, bool defdbg) : _defdbg(defdbg) {
    // Crea objeto
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Creando RelayFeedback en pin %d", fdb);
	_fdb = new InterruptIn(fdb);
	MBED_ASSERT(_fdb);
	_fdb->mode(mode);
    _level = level;
    _res = {0, 0, 0, (Status)0};
    _count = 0;
    _count_sc = 0;
    _tsc_acc = 0;
    _tsample = 0;
    _dbg_buf = {NULL, 0, 0};
    _flags = (Flags)0;
    _fdb->rise(NULL);
    _fdb->fall(NULL);
    _fdb_test_cb = NULL;
}


//------------------------------------------------------------------------------------
RelayFeedback::RelayFeedback(const InterruptIn& fdb, LogicLevel level, bool defdbg) : _defdbg(defdbg) {
    // Crea objeto
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Creando RelayFeedback");
	_fdb = (InterruptIn*)&fdb;
	MBED_ASSERT(_fdb);
    _level = level;
    _res = {0, 0, 0, (Status)0};
    _count = 0;
    _count_sc = 0;
    _tsc_acc = 0;
    _tsample = 0;
    _flags = (Flags)0;
    _dbg_buf = {NULL, 0, 0};
    _fdb->rise(NULL);
    _fdb->fall(NULL);
    _fdb_test_cb = NULL;
}


//------------------------------------------------------------------------------------
void RelayFeedback::start(){
	_dbg_buf.count = 0;
	_count = 0;
	_tsample = 0;
	_tsc_acc = 0;
	_count_sc = 0;
	_res = {0, 0, 0, (Status)0};
	_flags = (Flags)0;

	// Habilita el flanco de inicio y captura
	if(_level == ReleasedIsHighLevel){
		_fdb->rise(callback(this, &RelayFeedback::isrCaptureCallback));
		_fdb->fall(callback(this, &RelayFeedback::isrStartCallback));
	}
	else{
		_fdb->rise(callback(this, &RelayFeedback::isrStartCallback));
		_fdb->fall(callback(this, &RelayFeedback::isrCaptureCallback));
	}

}


//------------------------------------------------------------------------------------
void RelayFeedback::pause(){
	_flags = (Flags)(_flags | Paused);
}


//------------------------------------------------------------------------------------
void RelayFeedback::resume(){
	// Habilita flag
	_flags = (Flags)(_flags | Resumed);
	_flags = (Flags)(_flags & ~Paused);

	// Habilita el flanco de inicio y captura
	if(_level == ReleasedIsHighLevel){
		_fdb->rise(callback(this, &RelayFeedback::isrCaptureCallback));
		_fdb->fall(callback(this, &RelayFeedback::isrStartCallback));
	}
	else{
		_fdb->rise(callback(this, &RelayFeedback::isrStartCallback));
		_fdb->fall(callback(this, &RelayFeedback::isrCaptureCallback));
	}
}


//------------------------------------------------------------------------------------
void RelayFeedback::stop(){
	_flags = (Flags)(_flags | Stopped);
}


//------------------------------------------------------------------------------------
void RelayFeedback::enableDebugBuffer(uint32_t size){
	if(_dbg_buf.buf){
		Heap::memFree(_dbg_buf.buf);
		_dbg_buf.buf = NULL;
	}
	_dbg_buf.buf = (uint32_t*)Heap::memAlloc(size * sizeof(uint32_t));
	MBED_ASSERT(_dbg_buf.buf);
	_dbg_buf.count = 0;
	_dbg_buf.size = size;
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Buffer de depuración activado con %d muestras", _dbg_buf.size);
}


//------------------------------------------------------------------------------------
void RelayFeedback::printDebugBuffer(){
	MBED_ASSERT(_dbg_buf.buf);
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Imprimiendo buffer de %d datos:", _dbg_buf.count);
	for(int i=0;i<_dbg_buf.count; i++){
		DEBUG_TRACE_D(_EXPR_, _MODULE_, "[%d]=%d", i, _dbg_buf.buf[i]);
	}
	_dbg_buf.count = 0;
}


//------------------------------------------------------------------------------------
RelayFeedback::Status RelayFeedback::getResult(uint32_t* t_on_us, uint32_t* t_off_us, uint32_t *t_sc_us, uint32_t delta_us){
	_res.status = (Status)0;

	if(delta_us == 0){
		delta_us = (uint32_t)(DefaultDeltaPercent * _res.tsc/100);
	}
	if(_res.tOnUs >= _res.tsc){
		_res.status = (Status)(_res.status | ErrorTimeOnHigh);
	}
	if(_res.tOnUs < (_res.tsc - delta_us)){
		_res.status = (Status)(_res.status | ErrorTimeOnLow);
	}
	if(_res.tOffUs >= _res.tsc){
		_res.status = (Status)(_res.status | ErrorTimeOffHigh);
	}
	if(_res.tOffUs < (_res.tsc - delta_us)){
		_res.status = (Status)(_res.status | ErrorTimeOffLow);
	}

	*t_on_us = _res.tOnUs;
	*t_off_us = _res.tOffUs;
	*t_sc_us = _res.tsc;
	return _res.status;
}


//------------------------------------------------------------------------------------
//-- PRIVATE METHODS IMPLEMENTATION --------------------------------------------------
//------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------
void RelayFeedback::isrStartCallback(){
	_fdb_test_cb.call(0);
	_tmr.start();
	wait_us(DefaultGlitchTimeout);
}


//------------------------------------------------------------------------------------
void RelayFeedback::isrCaptureCallback(){
	_fdb_test_cb.call(1);
	uint32_t sample = _tmr.read_us();
	wait_us(DefaultGlitchTimeout);
	capture(sample);
}


//------------------------------------------------------------------------------------
void RelayFeedback::capture(uint32_t sample){
	// si ha sido pausado, desactiva isr
	if((_flags & (Paused|Stopped)) != 0){
		_fdb->rise(NULL);
		_fdb->fall(NULL);
		return;
	}
	// tras reanudar, salta la primera muestra
	if((_flags & Resumed) != 0){
		_flags = (Flags)(_flags & ~Resumed);
		return;
	}

	// si está habilitado el buffer de depuración, captura la muestra
	if(_dbg_buf.buf){
		if(_dbg_buf.count < _dbg_buf.size){
			_dbg_buf.buf[_dbg_buf.count++] = sample;
		}
	}
	// evalúa de forma normal
	switch(_count){
		case 0:{
			_res.tOnUs = sample;
			break;
		}
		case 1:{
			_tsc_acc = sample;
			_count_sc = 1;
			break;
		}
		default:{
			_count = 1;
			_tsc_acc += _tsample;
			_count_sc++;
			_res.tsc = _tsc_acc/_count_sc;
			_res.tOffUs = sample;
			break;
		}
	}
	_tsample = sample;
	_count++;
}

