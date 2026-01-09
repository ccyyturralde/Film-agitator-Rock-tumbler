// API base URL (will be same origin when served from ESP32)
const API_BASE = '/api';

// DOM elements
const statusEl = document.getElementById('status');
const rpmEl = document.getElementById('rpm');
const directionEl = document.getElementById('direction');
const rpmSlider = document.getElementById('rpmSlider');
const rpmValue = document.getElementById('rpmValue');
const dirForwardBtn = document.getElementById('dirForward');
const dirReverseBtn = document.getElementById('dirReverse');
const startBtn = document.getElementById('startBtn');
const stopBtn = document.getElementById('stopBtn');

// State
let currentRPM = 60;
let isRunning = false;
let direction = 'forward';
let statusCheckInterval = null;

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    // Set up event listeners
    rpmSlider.addEventListener('input', (e) => {
        currentRPM = parseInt(e.target.value);
        rpmValue.textContent = currentRPM;
    });
    
    dirForwardBtn.addEventListener('click', () => {
        setDirection('forward');
    });
    
    dirReverseBtn.addEventListener('click', () => {
        setDirection('reverse');
    });
    
    startBtn.addEventListener('click', () => {
        startMotor(currentRPM);
    });
    
    stopBtn.addEventListener('click', () => {
        stopMotor();
    });
    
    // Start status polling
    startStatusPolling();
    
    // Initial status check
    updateStatus();
});

async function updateStatus() {
    try {
        const response = await fetch(`${API_BASE}`);
        const data = await response.json();
        
        isRunning = data.running;
        direction = data.direction;
        
        // Update UI
        statusEl.textContent = isRunning ? 'Running' : 'Stopped';
        statusEl.className = `value ${isRunning ? 'running' : 'stopped'}`;
        rpmEl.textContent = Math.round(data.rpm);
        directionEl.textContent = data.direction.charAt(0).toUpperCase() + data.direction.slice(1);
        
        // Update direction buttons
        if (direction === 'forward') {
            dirForwardBtn.classList.add('active');
            dirReverseBtn.classList.remove('active');
        } else {
            dirForwardBtn.classList.remove('active');
            dirReverseBtn.classList.add('active');
        }
        
        // Update slider if motor is running at different RPM
        if (isRunning && data.rpm > 0) {
            currentRPM = data.rpm;
            rpmSlider.value = currentRPM;
            rpmValue.textContent = Math.round(currentRPM);
        }
    } catch (error) {
        console.error('Error updating status:', error);
        statusEl.textContent = 'Error';
        statusEl.className = 'value stopped';
    }
}

async function startMotor(rpm) {
    try {
        const formData = new URLSearchParams();
        formData.append('action', 'start');
        formData.append('rpm', rpm.toString());
        
        const response = await fetch(`${API_BASE}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: formData.toString()
        });
        
        if (response.ok) {
            const data = await response.json();
            updateUIFromResponse(data);
            console.log('Motor started at', rpm, 'RPM');
        } else {
            console.error('Failed to start motor');
            alert('Failed to start motor. Please try again.');
        }
    } catch (error) {
        console.error('Error starting motor:', error);
        alert('Error starting motor. Check connection.');
    }
}

async function stopMotor() {
    try {
        const formData = new URLSearchParams();
        formData.append('action', 'stop');
        
        const response = await fetch(`${API_BASE}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: formData.toString()
        });
        
        if (response.ok) {
            const data = await response.json();
            updateUIFromResponse(data);
            console.log('Motor stopped');
        } else {
            console.error('Failed to stop motor');
            alert('Failed to stop motor. Please try again.');
        }
    } catch (error) {
        console.error('Error stopping motor:', error);
        alert('Error stopping motor. Check connection.');
    }
}

async function setDirection(dir) {
    try {
        const formData = new URLSearchParams();
        formData.append('action', 'direction');
        formData.append('dir', dir);
        
        const response = await fetch(`${API_BASE}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: formData.toString()
        });
        
        if (response.ok) {
            const data = await response.json();
            updateUIFromResponse(data);
            console.log('Direction set to', dir);
        } else {
            console.error('Failed to set direction');
        }
    } catch (error) {
        console.error('Error setting direction:', error);
    }
}

function updateUIFromResponse(data) {
    isRunning = data.running;
    direction = data.direction;
    
    statusEl.textContent = isRunning ? 'Running' : 'Stopped';
    statusEl.className = `value ${isRunning ? 'running' : 'stopped'}`;
    rpmEl.textContent = Math.round(data.rpm);
    directionEl.textContent = data.direction.charAt(0).toUpperCase() + data.direction.slice(1);
    
    if (direction === 'forward') {
        dirForwardBtn.classList.add('active');
        dirReverseBtn.classList.remove('active');
    } else {
        dirForwardBtn.classList.remove('active');
        dirReverseBtn.classList.add('active');
    }
}

function startStatusPolling() {
    // Poll status every 500ms
    statusCheckInterval = setInterval(updateStatus, 500);
}

// Prevent accidental page refresh during operation
window.addEventListener('beforeunload', (e) => {
    if (isRunning) {
        e.preventDefault();
        e.returnValue = '';
    }
});
