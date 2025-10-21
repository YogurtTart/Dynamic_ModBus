function showStatus(message, type) {
    const status = document.getElementById('status');
    
    // Remove any existing hiding class
    status.classList.remove('hiding');
    
    // Set message content and styling
    status.textContent = message;
    status.className = 'status-message ' + type;
    
    // Show the message with animation
    status.style.display = 'block';
    
    // Auto-hide after 5 seconds with animation
    setTimeout(() => {
        status.classList.add('hiding');
        setTimeout(() => {
            status.style.display = 'none';
            status.classList.remove('hiding');
        }, 300);
    }, 5000);
}

async function loadSettings() {
    try {
        const response = await fetch('/getwifi');
        const data = await response.json();
        
        document.getElementById('sta_ssid').value = data.sta_ssid || '';
        document.getElementById('sta_password').value = data.sta_password || '';
        document.getElementById('ap_ssid').value = data.ap_ssid || '';
        document.getElementById('ap_password').value = data.ap_password || '';
        
        showStatus('WiFi settings loaded successfully!', 'success');
    } catch (error) {
        showStatus('Error loading WiFi settings: ' + error, 'error');
    }
}

async function saveSettings() {
    const formData = new FormData();
    formData.append('sta_ssid', document.getElementById('sta_ssid').value);
    formData.append('sta_password', document.getElementById('sta_password').value);
    formData.append('ap_ssid', document.getElementById('ap_ssid').value);
    formData.append('ap_password', document.getElementById('ap_password').value);
    
    try {
        const response = await fetch('/savewifi', {
            method: 'POST',
            body: formData
        });
        
        const result = await response.text();
        showStatus('WiFi settings saved successfully! Device will use new settings on restart.', 'success');
        
    } catch (error) {
        showStatus('Error saving WiFi settings: ' + error, 'error');
    }
}

// Navigation functions for the new pages
function navigateToSettings() {
    window.location.href = '/settings.html';
}

function navigateToDebug() {
    window.location.href = '/debug.html';
}

function navigateToSlaves() {
    window.location.href = '/slaves.html';
}

function navigateToWifi() {
    window.location.href = '/';
}

// Add these functions to script.js

async function loadIPInfo() {
    try {
        const response = await fetch('/getipinfo');
        const data = await response.json();
        
        // Update STA information
        document.getElementById('sta_ip').textContent = data.sta_ip;
        document.getElementById('sta_subnet').textContent = data.sta_subnet;
        document.getElementById('sta_gateway').textContent = data.sta_gateway;
        
        const staStatus = document.getElementById('sta_status');
        if (data.sta_connected) {
            staStatus.textContent = 'Connected';
            staStatus.className = 'ip-value status-connected';
        } else {
            staStatus.textContent = 'Disconnected';
            staStatus.className = 'ip-value status-disconnected';
        }
        
        // Update AP information
        document.getElementById('ap_ip').textContent = data.ap_ip;
        document.getElementById('ap_clients').textContent = 
            data.ap_connected_clients + ' client(s)';
            
    } catch (error) {
        console.error('Error loading IP info:', error);
        showStatus('Error loading network status', 'error');
    }
}

async function refreshIPInfo() {
    showStatus('Refreshing network status...', 'info');
    await loadIPInfo();
    showStatus('Network status updated', 'success');
}

// Update the DOMContentLoaded event to also load IP info
document.addEventListener('DOMContentLoaded', function() {
    loadSettings();
    loadIPInfo(); // Load IP info on page load
    
    // Auto-refresh IP info every 30 seconds
    setInterval(loadIPInfo, 30000);
});

// Load settings when page loads
document.addEventListener('DOMContentLoaded', loadSettings);