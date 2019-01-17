/*
 * RelayFeedback.h
 *
 *  Created on: Sep 2017
 *      Author: raulMrello
 *
 *	RelayFeedback es el driver encargado de gestionar el feedback de la conmutaci�n del rel�, para asegurar que la conumtaci�n
 *	se realiza lo m�s cerca posible del paso por cero. Es un sistema din�mico que se auto-ajusta de forma constante en cada
 *	conmutaci�n de rel�. Cuenta con una entrada digital de este estilo:
 *
 *	               ON                        OFF
 *	----------------      ----      ----      -----------------------
 *					|    |    |    |    |    |
 *				     ----      ----      ----
 *                    tf                  tl
 *
 *  La calibaci�n utiliza los tiempos tf y tl de forma que:
 *
 *	AJUSTE DE ON
 *  tf = Tsc (tiempo de semiciclo) => ERROR hay que reajustar (incrementando Ton)
 *  tf < Tsc => ADJ hay que reajustar (decrementando Ton hasta tf = (Tsc - delta) siendo delta un porcentaje m�nimo de Tsc
 *  tf < Tsc && tf >= Tsc-delta => CALIBRADO
 *
 *	AJUSTE DE OFF
 *  tl = Tsc (tiempo de semiciclo) => ERROR hay que reajustar (decrementando Toff)
 *  tl < Tsc => ADJ hay que reajustar (incrementando Toff hasta tl = (Tsc - delta) siendo delta un porcentaje m�nimo de Tsc
 *  tl < Tsc && tl >= Tsc-delta => CALIBRADO
 *
 */
 
#ifndef __RelayFeedback__H
#define __RelayFeedback__H

#include "mbed.h"

   
class RelayFeedback {
  public:
    
	/** Configuraci�n del flanco activo para la detecci�n.
	 */
    enum LogicLevel{
        ReleasedIsLowLevel,//!< ReleasedIsLowLevel El flanco activo es el de subida
        ReleasedIsHighLevel//!< ReleasedIsHighLevel El flanco activo es el de bajada
    };
    
    /** Flags de estado en condiciones de error
     *
     */
    enum Status{
    	ErrorTimeOnHigh	 = (1 << 0), 	//!< ErrorTimeOnHigh Tiempo tf >= Tsc
		ErrorTimeOnLow 	 = (1 << 1),   	//!< ErrorTimeOnLow Tiempo tf < (Tsc - delay)
    	ErrorTimeOffHigh = (1 << 2),	//!< ErrorTimeOffHigh Tiempo tl >= Tsc
		ErrorTimeOffLow  = (1 << 3),   	//!< ErrorTimeOffLow Tiempo tl <(Tsc - delay)
    };

	/** Constructor a partir de un pin dado
	 *
	 * @param fdb Pin feedback utilizado
	 * @param level Nivel l�gico del pin
	 * @param mode Configuraci�n del pin (pullup)
	 * @param defdbg Flag para habilitar depuraci�n
	 */
    RelayFeedback(PinName fdb, LogicLevel level, PinMode mode, bool defdbg = false);


	/** Constructor a partir de un objeto InterruptIn ya existente
	 *
	 * @param fdb Pin feedback utilizado
	 * @param level Nivel l�gico del pin
	 * @param defdbg Flag para habilitar depuraci�n
	 */
    RelayFeedback(const InterruptIn& fdb, LogicLevel level, bool defdbg = false);


    /** Destructor
     */
    ~RelayFeedback();
    

	/** Lee los tiempos de ON y de OFF
     *
     *  @param t_on_us Recibe el tiempo de ON en microseg
     *  @param t_off_us Recibe el tiempo de OFF en microseg
     *  @param t_sc_us Recibe el tiempo del semiciclo en microseg
     *	@return Status con los flags de resultado
     */
    Status getResult(uint32_t* t_on_us, uint32_t* t_off_us, uint32_t *t_sc_us, uint32_t delta_us);
  
    
	/** Inicia la captura
     *
     */
    void start();


    /** Pausa la captura. Una vez pausado, no toma efecto hasta el siguiente flanco inactivo
     *
     */
    void pause();


    /** Reinicia la captura. Una vez hecho, no toma efecto hasta el siguiente flanco activo.
     *
     */
    void resume();

	/** Detiene la captura
     *
     */
    void stop();


    /** Habilita un buffer de depuraci�n para guardar las muestras obtenidas tras una conmutaci�n
     *
     * @param size Tama�o del buffer
     */
    void enableDebugBuffer(uint32_t size);


    /** Imprime las muestras en el buffer de depuraci�n
     *
     */
    void printDebugBuffer();


    /** Instala una callback para testear la operativa del pin feedback
     *
     * @param fdbTesterCb Callback a instalar
     */
    void attachFeedbackTester(Callback<void(uint8_t)> fdbTesterCb){
    	MBED_ASSERT(fdbTesterCb);
    	_fdb_test_cb = fdbTesterCb;
    }


    /** Porcentaje por defecto del delta de comparaci�n de resultado cuando no ha sido a�n calibrado el feedback (5%) */
    static const uint32_t DefaultDeltaPercent = 5;


    /** Tiempo en millis para habilitar la captura del feedback antes de la conmutaci�n del rel� */
    static const uint32_t DefaultPreviousCaptureTime = 100;

  private:

    /** Tiempo en microseg del filtro antiglitch en el feedback */
    static const uint32_t DefaultGlitchTimeout = 500;

    /** Flags de estado de funcionamiento
     *
     */
    enum Flags{
    	PauseRequest	= (1 << 0), 	//!< Flag para notificar que se desea detener la captura
    	Paused			= (1 << 1),		//!< Flag para notificar estado pausado
    	ResumeRequest	= (1 << 2), 	//!< Flag para notificar que se desea continuar con la captura
    	Resumed			= (1 << 3),		//!< Flag para notificar que se ha continuado con la captura
    	Stopped			= (1 << 4),		//!< Flag para notificar que se ha detenido la captura

    };


    /** Estructura con los par�metros de configuraci�n
     *
     */
    struct Result_t{
    	uint32_t tsc;			/// Tiempo del semiciclo en microseg
    	uint32_t tOnUs;			/// Tiempo de ON en microseg
    	uint32_t tOffUs;		/// Tiempo de OFF en microseg
    	Status status;			/// Flags de resultado de estado
    };


    /** Estructura con los par�metros para un buffer de depuraci�n
     *
     */
    struct Buffer_t{
    	uint32_t *buf;
    	uint32_t size;
    	uint32_t count;
    };


    Result_t _res;					/// Variable de resultado
    Flags _flags;					/// Flags de estado
    InterruptIn* _fdb;				/// InterruptIn asociada
    LogicLevel _level;          	/// Nivel l�gico
    Timer _tmr;						/// Timer de alta resoluci�n (ns)
    bool _defdbg;					/// Flag para activar las trazas de depuraci�n por defecto
    uint32_t _tsample;				/// Array con las muestras de tf, tsc, tl durante la conmutaci�n
    uint8_t _count;					/// Contador de muestras capturadas
    uint32_t _tsc_acc;				/// Valor acumulado de muestras Tsc para calcular la media
    uint32_t _count_sc;				/// N�mero de muestras Tsc capturadas
    Buffer_t _dbg_buf;				/// Buffer de depuraci�n
    Callback<void(uint8_t)> _fdb_test_cb;	/// Callback para testear operativa en pin feedback


	/** isrStartCallback
     *  ISR para iniciar la medida del semiciclo
     */
    void isrStartCallback();
    
	/** isrCaptureCallback
     *  ISR para procesar la medida del semiciclo
     */
    void isrCaptureCallback();


    /** Captura el tiempo asociado a la �ltima muestra tomada
     *
     *	@param sample �ltima muestra tomada con la medida en microseg del semiciclo activo
     */
    void capture(uint32_t sample);
  
};
     


#endif /*__RelayFeedback__H */

/**** END OF FILE ****/


