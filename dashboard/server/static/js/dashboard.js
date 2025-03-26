// Connect to WebSocket server
const socket = io();

// DOM elements
const speedValue = document.getElementById('speed-value');
const throttleValue = document.getElementById('throttle-value');
const batteryValue = document.getElementById('battery-value');
const tempValue = document.getElementById('temp-value');
const brakeValue = document.getElementById('brake-value');
const steeringValue = document.getElementById('steering-value');

const speedControl = document.getElementById('speed-control');
const steeringControl = document.getElementById('steering-control');
const brakeControl = document.getElementById('brake-control');
const speedDisplay = document.getElementById('speed-display');
const steeringDisplay = document.getElementById('steering-display');
const brakeDisplay = document.getElementById('brake-display');

const emergencyStopBtn = document.getElementById('emergency-stop');
const sendCommandsBtn = document.getElementById('send-commands');

// Camera elements
const cameraPanel = document.getElementById('camera-panel');
const cameraFeed = document.getElementById('camera-feed');
const cameraStatus = document.getElementById('camera-status');
const cameraToggle = document.getElementById('camera-toggle');
const cameraSnapshot = document.getElementById('camera-snapshot');
let cameraActive = true;

// Get simulation mode toggle and status
const simulationToggle = document.getElementById('simulation-toggle');
const simulationStatus = document.getElementById('simulation-status');

// Hide simulation control
simulationToggle.parentElement.parentElement.parentElement.style.display = 'none';

// Update displays when controls change
speedControl.addEventListener('input', () => {
    speedDisplay.textContent = speedControl.value;
});

steeringControl.addEventListener('input', () => {
    steeringDisplay.textContent = steeringControl.value;
});

brakeControl.addEventListener('input', () => {
    brakeDisplay.textContent = brakeControl.value;
});

// Helper function for sending commands
function sendCommand(componentType, componentName, commandName, value) {
    // Validate value is an integer
    if (!Number.isInteger(value)) {
        console.error(`Invalid value for ${componentName}.${commandName}: ${value}`);
        return Promise.reject(new Error('Value must be an integer'));
    }
    
    return fetch('/api/command', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            component_type: componentType,
            component_name: componentName,
            command_name: commandName,
            direct_value: value
        }),
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`Server returned ${response.status}: ${response.statusText}`);
        }
        return response.json();
    })
    .catch(error => {
        console.error(`Error sending ${componentName} command:`, error);
        throw error; // Re-throw for handling by caller
    });
}

// Emergency stop button
let emergencyActive = false;
emergencyStopBtn.addEventListener('click', () => {
    emergencyActive = !emergencyActive;
    
    if (emergencyActive) {
        emergencyStopBtn.textContent = 'RELEASE EMERGENCY STOP';
        emergencyStopBtn.style.backgroundColor = '#d13535';
    } else {
        emergencyStopBtn.textContent = 'EMERGENCY STOP';
        emergencyStopBtn.style.backgroundColor = '#ff4a4a';
    }
    
    // Send command using the correct format
    sendCommand('CONTROLS', 'EMERGENCY', 'STOP', emergencyActive ? 1 : 0);
});

// Send commands button
sendCommandsBtn.addEventListener('click', () => {
    // Send multiple commands - speed, steering, and brake
    sendCommand('CONTROLS', 'THROTTLE', 'SPEED', parseInt(speedControl.value));
    sendCommand('CONTROLS', 'STEERING', 'ANGLE', parseInt(steeringControl.value));
    sendCommand('CONTROLS', 'BRAKE', 'PRESSURE', parseInt(brakeControl.value));
});

// Setup Chart.js
const ctx = document.getElementById('historyChart').getContext('2d');
const chart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [
            {
                label: 'Speed (km/h)',
                borderColor: 'rgb(75, 192, 192)',
                data: [],
                pointRadius: 0,
                borderWidth: 2
            },
            {
                label: 'Motor Temp (°C)',
                borderColor: 'rgb(255, 99, 132)',
                data: [],
                pointRadius: 0,
                borderWidth: 2
            },
            {
                label: 'Battery (V)',
                borderColor: 'rgb(54, 162, 235)',
                data: [],
                pointRadius: 0,
                borderWidth: 2
            }
        ]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: {
            duration: 0 // Disable animations for better performance
        },
        elements: {
            line: {
                tension: 0.2 // Slight curve for better visualization
            }
        },
        scales: {
            x: {
                display: true,
                title: {
                    display: true,
                    text: 'Time'
                },
                ticks: {
                    maxTicksLimit: 10, // Limit X-axis labels for readability
                    autoSkip: true
                }
            },
            y: {
                display: true,
                title: {
                    display: true,
                    text: 'Value'
                },
                beginAtZero: true
            }
        },
        plugins: {
            legend: {
                position: 'top'
            }
        }
    }
});

// Fetch historical data on load
fetch('/api/telemetry/history')
    .then(response => response.json())
    .then(data => {
        if (data.length === 0) return;
        
        // Sort data by timestamp to ensure chronological order
        data.sort((a, b) => a.timestamp - b.timestamp);
        
        // Format timestamps for x-axis
        const labels = data.map(item => {
            const date = new Date(item.timestamp * 1000);
            return date.toLocaleTimeString();
        });
        
        // Extract data series
        const speedData = data.map(item => item.speed);
        const tempData = data.map(item => item.motor_temp);
        const batteryData = data.map(item => item.battery_voltage);
        
        // Update chart
        chart.data.labels = labels;
        chart.data.datasets[0].data = speedData;
        chart.data.datasets[1].data = tempData;
        chart.data.datasets[2].data = batteryData;
        chart.update('none');
        
        console.log(`Loaded ${data.length} historical data points`);
    })
    .catch(error => {
        console.error('Error fetching historical data:', error);
    });

// Listen for real-time updates
socket.on('state_update', (state) => {
    // Update dashboard metrics
    speedValue.textContent = parseFloat(state.speed).toFixed(2);
    throttleValue.textContent = parseInt(state.throttle);
    batteryValue.textContent = parseFloat(state.battery_voltage).toFixed(2);
    tempValue.textContent = parseFloat(state.motor_temp).toFixed(2);
    brakeValue.textContent = parseFloat(state.brake_pressure).toFixed(2);
    steeringValue.textContent = parseFloat(state.steering_angle).toFixed(2);
    
    // Update chart
    const timestamp = new Date(state.timestamp * 1000).toLocaleTimeString();
    
    // Add new data point
    chart.data.labels.push(timestamp);
    chart.data.datasets[0].data.push(state.speed);
    chart.data.datasets[1].data.push(state.motor_temp);
    chart.data.datasets[2].data.push(state.battery_voltage);
    
    // Keep only last 60 seconds of data in chart
    if (chart.data.labels.length > 600) {
        chart.data.labels.shift();
        chart.data.datasets.forEach(dataset => {
            dataset.data.shift();
        });
    }
    
    // Force complete redraw on each update
    chart.update('none');
});

// Check camera availability
fetch('/api/camera/status')
    .then(response => response.json())
    .then(data => {
        if (!data.available) {
            // Hide camera panel if camera is not available
            cameraPanel.style.display = 'none';
            console.log('Camera not available');
        } else {
            cameraStatus.textContent = 'Camera Ready';
            setTimeout(() => {
                cameraStatus.style.display = 'none';
            }, 3000);
            console.log('Camera available:', data);
        }
    })
    .catch(error => {
        console.error('Error checking camera status:', error);
        cameraPanel.style.display = 'none';
    });

// Handle camera controls
socket.on('camera_frame', (data) => {
    if (cameraActive) {
        cameraFeed.src = 'data:image/jpeg;base64,' + data.image;
        if (cameraStatus.style.display !== 'none') {
            cameraStatus.style.display = 'none';
        }
    }
});

cameraToggle.addEventListener('click', () => {
    cameraActive = !cameraActive;
    cameraToggle.textContent = cameraActive ? 'Pause Feed' : 'Resume Feed';
    
    if (!cameraActive) {
        // Show last frame but grayed out
        cameraFeed.style.filter = 'grayscale(100%)';
    } else {
        cameraFeed.style.filter = 'none';
    }
});

cameraSnapshot.addEventListener('click', () => {
    // Create a download link for the current frame
    const link = document.createElement('a');
    link.download = 'gokart-snapshot-' + new Date().toISOString().replace(/:/g, '-') + '.jpg';
    link.href = cameraFeed.src;
    link.click();
});

// Light control elements
const lightModeButtons = document.querySelectorAll('.light-mode-btn');
const signalButtons = document.querySelectorAll('.signal-btn');
const locationButtons = document.querySelectorAll('.location-btn');
const brakeToggle = document.getElementById('brake-toggle');
const brakeStatus = document.getElementById('brake-status');
const testToggle = document.getElementById('test-toggle');
const testStatus = document.getElementById('test-status');

// Light mode control
lightModeButtons.forEach((button, index) => {
    button.addEventListener('click', () => {
        lightModeButtons.forEach(btn => btn.classList.remove('active'));
        button.classList.add('active');
        
        fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                component_type: 'LIGHTS',
                component_name: 'ALL',
                command_name: 'MODE',
                direct_value: index
            }),
        });
    });
});

// Turn signal control
signalButtons.forEach((button, index) => {
    button.addEventListener('click', () => {
        signalButtons.forEach(btn => btn.classList.remove('active'));
        button.classList.add('active');
        
        fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                component_type: 'LIGHTS',
                component_name: 'ALL',
                command_name: 'SIGNAL',
                direct_value: index
            }),
        });
    });
});

// Brake lights control
brakeToggle.addEventListener('change', () => {
    const isOn = brakeToggle.checked;
    brakeStatus.textContent = isOn ? 'On' : 'Off';
    
    fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                component_type: 'LIGHTS',
                component_name: 'ALL',
                command_name: 'BRAKE',
                direct_value: isOn ? 1 : 0
            }),
        });
});

// Test mode control
testToggle.addEventListener('change', () => {
    const isOn = testToggle.checked;
    testStatus.textContent = isOn ? 'On' : 'Off';
    
    fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                component_type: 'CONTROLS',
                component_name: 'DIAGNOSTIC',
                command_name: 'MODE',
                direct_value: isOn ? 1 : 0
            }),
        });
});

// Location control
locationButtons.forEach((button, index) => {
    button.addEventListener('click', () => {
        locationButtons.forEach(btn => btn.classList.remove('active'));
        button.classList.add('active');
        
        fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                component_type: 'LIGHTS',
                component_name: 'ALL',
                command_name: 'LOCATION',
                direct_value: index === 0 ? 0 : 1 // 0 for FRONT, 1 for REAR
            }),
        });
    });
});

// Update light controls from state updates
socket.on('state_update', (state) => {
    // Existing state updates...
    
    // Update light controls
    if ('light_mode' in state) {
        lightModeButtons.forEach((btn, idx) => {
            btn.classList.toggle('active', idx === state.light_mode);
        });
    }
    
    if ('light_signal' in state) {
        signalButtons.forEach((btn, idx) => {
            btn.classList.toggle('active', idx === state.light_signal);
        });
    }
    
    if ('light_brake' in state) {
        brakeToggle.checked = state.light_brake === 1;
        brakeStatus.textContent = state.light_brake === 1 ? 'On' : 'Off';
    }
    
    if ('light_test' in state) {
        testToggle.checked = state.light_test === 1;
        testStatus.textContent = state.light_test === 1 ? 'On' : 'Off';
    }
}); 