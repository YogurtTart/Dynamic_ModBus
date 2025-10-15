function showStatus(message, type) {
    const status = document.getElementById('status');
    status.textContent = message;
    status.className = 'status-message ' + type;
    status.style.display = 'block';
    
    setTimeout(() => {
        status.style.display = 'none';
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

// Load settings when page loads
document.addEventListener('DOMContentLoaded', loadSettings);