document.addEventListener('DOMContentLoaded', () => {
    // Inicialización de todos los componentes
    initCredenciales();
    initTemperatura();
    initLED();
    initPotenciometro();
    
    // Botones Volver
    document.querySelectorAll('.boton-volver').forEach(btn => {
        btn.addEventListener('click', () => {
            window.location.href = 'index.html';
        });
    });
});


function initTemperatura() {
    if (document.querySelector('.pagina-temperatura')) {
        let max = -Infinity;
        let min = Infinity;
        let isUpdating = false;
        let intervalId = null;

        const elements = {
            gaugeArc: document.querySelector('.gauge-arc'),
            gaugeText: document.querySelector('.gauge-text'),
            tempActual: document.getElementById('temperatura-actual'),
            tempMaxC: document.getElementById('temp-max-c'),
            tempMaxF: document.getElementById('temp-max-f'),
            tempMinC: document.getElementById('temp-min-c'),
            tempMinF: document.getElementById('temp-min-f'),
            toggleBtn: document.getElementById('toggle-lectura')
        };

        function actualizarColor(temp) {
            const colors = {
                cold: '#2196F3',
                optimal: '#4CAF50',
                warm: '#FFC107',
                hot: '#F44336'
            };
            elements.gaugeArc.style.stroke = temp < 20 ? colors.cold : 
                                          temp < 25 ? colors.optimal :
                                          temp < 30 ? colors.warm : colors.hot;
        }

        function actualizarDesdeServidor() {
            fetch('/temp_adc')
                .then(response => response.json())
                .then(data => {
                    const tempC = data.temp;
                    const tempF = (tempC * 9/5 + 32).toFixed(1);
                    
                    // Actualizar gauge
                    const circunferencia = 2 * Math.PI * 45;
                    elements.gaugeArc.style.strokeDasharray = 
                        `${((tempC - 15)/30 * circunferencia)} ${circunferencia}`;
                    elements.gaugeText.textContent = `${tempC.toFixed(1)}°C`;
                    
                    // Actualizar display principal
                    elements.tempActual.textContent = tempF;
                    
                    // Actualizar máximos y mínimos
                    max = Math.max(max, tempC);
                    min = Math.min(min, tempC);
                    
                    elements.tempMaxC.textContent = max.toFixed(1);
                    elements.tempMinC.textContent = min.toFixed(1);
                    elements.tempMaxF.textContent = (max * 9/5 + 32).toFixed(1);
                    elements.tempMinF.textContent = (min * 9/5 + 32).toFixed(1);
                    
                    actualizarColor(tempC);
                })
                .catch(error => console.error('Error:', error));
        }

        // Control del botón
        elements.toggleBtn.addEventListener('click', () => {
            isUpdating = !isUpdating;
            elements.toggleBtn.textContent = isUpdating ? 'Detener Lectura' : 'Iniciar Lectura';
            
            if (isUpdating) {
                actualizarDesdeServidor(); // Primera actualización inmediata
                intervalId = setInterval(actualizarDesdeServidor, 2000);
            } else {
                clearInterval(intervalId);
            }
        });

        // Valores iniciales
        actualizarColor(25); // Temperatura inicial por defecto
    }
}



function initLED() {
    if(document.querySelector('.pagina-led')) {
        let colorActual = { rojo: 255, verde: 0, azul: 0 };

        function actualizarValores() {
            const rojo = document.getElementById('rojo').value;
            const verde = document.getElementById('verde').value;
            const azul = document.getElementById('azul').value;
            
            colorActual = { rojo, verde, azul };
            
            document.getElementById('valor-rojo').textContent = rojo;
            document.getElementById('valor-verde').textContent = verde;
            document.getElementById('valor-azul').textContent = azul;
            
            const colorHex = `#${parseInt(rojo).toString(16).padStart(2, '0')}${parseInt(verde).toString(16).padStart(2, '0')}${parseInt(azul).toString(16).padStart(2, '0')}`;
            document.getElementById('color-preview').style.backgroundColor = colorHex;
            document.getElementById('color-picker').value = colorHex;
        }

        document.querySelectorAll('.slider').forEach(slider => {
            slider.addEventListener('input', actualizarValores);
        });
        
        document.getElementById('color-picker').addEventListener('input', (e) => {
            const hex = e.target.value;
            const rgb = hexToRgb(hex);
            document.getElementById('rojo').value = rgb.r;
            document.getElementById('verde').value = rgb.g;
            document.getElementById('azul').value = rgb.b;
            actualizarValores();
        });

        document.getElementById('btn-actualizar').addEventListener('click', () => {
            const timeOn = document.getElementById('time-on').value;
            const timeOff = document.getElementById('time-off').value;
            const rojo = document.getElementById('rojo').value;
            const verde = document.getElementById('verde').value;
            const azul = document.getElementById('azul').value;
            
            // Enviar valores al ESP32
            fetch('/actu_led', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    R: parseInt(rojo),
                    G: parseInt(verde),
                    B: parseInt(azul),
                    t_on: parseInt(timeOn),
                    t_off: parseInt(timeOff)
                })
            })
            .then(response => {
                if (!response.ok) throw new Error('Error en la respuesta');
                console.log('LED actualizado correctamente');
            })
            .catch(error => console.error('Error:', error));
        });

        actualizarValores();
    }
}

function initCredenciales() {
    const formCredenciales = document.getElementById('form-credenciales');
    if(formCredenciales) {
        document.querySelectorAll('.contenedor-ojo').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const inputId = e.currentTarget.dataset.target;
                mostrarPassword(inputId);
            });
        });

        formCredenciales.addEventListener('submit', (e) => {
            e.preventDefault();
            const formData = new FormData(e.target);
            const datos = Object.fromEntries(formData.entries());
            
            // Enviar credenciales al ESP32
            fetch('/update_credenciales', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: new URLSearchParams(formData)
            })
            .then(response => {
                if (response.ok) {
                    alert('Credenciales actualizadas correctamente');
                    window.location.reload();
                } else {
                    throw new Error('Error en la actualización');
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Error al actualizar credenciales');
            });
        });
    }
}

function initPotenciometro() {
    if(document.querySelector('.pagina-potenciometro')) {
        let isReading = false;
        let intervalId = null;
        let chart = null;
        const data = {
            labels: [],
            datasets: [{
                label: 'Voltaje (V)',
                data: [],
                borderColor: '#4CAF50',
                tension: 0.3,
                fill: true,
                backgroundColor: 'rgba(76, 175, 80, 0.2)'
            }]
        };

        // Configuración de la gráfica
        const ctx = document.getElementById('grafica-pot').getContext('2d');
        chart = new Chart(ctx, {
            type: 'line',
            data: data,
            options: {
                responsive: true,
                scales: {
                    y: {
                        min: 0,
                        max: 5,
                        title: {
                            display: true,
                            text: 'Voltios (V)'
                        }
                    },
                    x: {
                        display: false
                    }
                }
            }
        });

        // Elementos del DOM
        const toggleBtn = document.getElementById('toggle-lectura-pot');
        const aguja = document.getElementById('aguja-pot');
        const valorDigital = document.getElementById('valor-pot');

        // Generar valor aleatorio de voltaje (0-5V)
        function generarVoltaje() {
            return (Math.random() * 5).toFixed(2);
        }

        function actualizarDatos() {
            if (!isReading) return;
            
            const voltaje = generarVoltaje();
            const timestamp = new Date().toLocaleTimeString();
            
            // Actualizar gráfica
            if (data.labels.length > 20) {
                data.labels.shift();
                data.datasets[0].data.shift();
            }
            data.labels.push(timestamp);
            data.datasets[0].data.push(voltaje);
            chart.update();
            
            // Actualizar indicadores
            const grados = (voltaje / 5) * 270 - 45; // Convertir 0-5V a -45° a 225°
            aguja.style.transform = `rotate(${grados}deg)`;
            valorDigital.textContent = voltaje;
        }

        // Control del botón
        toggleBtn.addEventListener('click', () => {
            isReading = !isReading;
            toggleBtn.textContent = isReading ? 'Detener Lectura' : 'Iniciar Lectura';
            
            if (isReading) {
                intervalId = setInterval(actualizarDatos, 1000);
            } else {
                clearInterval(intervalId);
            }
        });

        // Valores iniciales
        aguja.style.transform = `rotate(-45deg)`;
    }
}

// Funciones utilitarias
function hexToRgb(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
        r: parseInt(result[1], 16),
        g: parseInt(result[2], 16),
        b: parseInt(result[3], 16)
    } : null;
}

function mostrarPassword(inputId) {
    const input = document.getElementById(inputId);
    const eyeIcon = input.parentElement.querySelector('.eye-icon'); // Selector corregido
    
    if(input.type === 'password') {
        input.type = 'text';
        eyeIcon.classList.add('mostrar');
    } else {
        input.type = 'password';
        eyeIcon.classList.remove('mostrar');
    }
}