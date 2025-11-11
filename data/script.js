// script.js - WiFi configuration page
class WifiManager {
    constructor() {
        this.init();
    }

    init() {
        this.bindEvents();
        this.loadInitialData();
        this.startAutoRefresh();
    }

    bindEvents() {
        // Enter key support for WiFi form
        const wifiForm = document.getElementById('wifiForm');
        if (wifiForm) {
            wifiForm.addEventListener('keypress', (e) => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    this.saveSettings();
                }
            });
        }
    }

    async loadInitialData() {
        try {
            await Promise.all([
                this.loadSettings(),
                this.loadIPInfo()
            ]);
        } catch (error) {
            console.error('WiFi initialization error:', error);
        }
    }

    startAutoRefresh() {
        // Auto-refresh IP info every 5 seconds (more frequent for connection status)
        setInterval(() => this.loadIPInfo(), 5000);
    }

    async loadSettings() {
        try {
            const data = await ApiClient.get('/getwifi');
            
            // Set form values
            const fields = {
                'sta_ssid': data.sta_ssid,
                'sta_password': data.sta_password,
                'ap_ssid': data.ap_ssid,
                'ap_password': data.ap_password,
                'mqtt_server': data.mqtt_server,
                'mqtt_port': data.mqtt_port
            };
            
            Object.entries(fields).forEach(([id, value]) => {
                FormHelper.setValue(id, value);
            });
            
        } catch (error) {
            // Error handled by ApiClient
        }
    }

    async saveSettings() {
        const requiredFields = [
            'sta_ssid', 'sta_password', 'ap_ssid', 
            'ap_password', 'mqtt_server', 'mqtt_port'
        ];
        
        if (!FormHelper.validateRequired(requiredFields)) return;

        const formData = new FormData();
        requiredFields.forEach(field => {
            formData.append(field, FormHelper.getValue(field));
        });

        try {
            await ApiClient.postForm('/savewifi', formData);
            StatusManager.showStatus(
                'WiFi & MQTT settings saved successfully!', 
                'success'
            );
        } catch (error) {
            // Error handled by ApiClient
        }
    }

    async loadIPInfo() {
        try {
            const data = await ApiClient.get('/getipinfo');
            this.updateIPDisplay(data);
        } catch (error) {
            // Error handled by ApiClient
        }
    }

    updateIPDisplay(data) {
        // STA information
        document.getElementById('sta_ip').textContent = data.sta_ip || 'N/A';
        document.getElementById('sta_subnet').textContent = data.sta_subnet || 'N/A';
        document.getElementById('sta_gateway').textContent = data.sta_gateway || 'N/A';
        
        const staStatus = document.getElementById('sta_status');
        const staStatusBadge = document.getElementById('sta_status_badge');
        
        // Handle different connection states
        if (data.sta_connecting) {
            staStatus.textContent = 'Connecting...';
            staStatus.className = 'ip-value status-connecting';
            staStatusBadge.textContent = 'Connecting';
            staStatusBadge.className = 'badge status-connecting';
        } else if (data.sta_connected) {
            staStatus.textContent = 'Connected';
            staStatus.className = 'ip-value status-connected';
            staStatusBadge.textContent = 'Connected';
            staStatusBadge.className = 'badge status-connected';
        } else {
            staStatus.textContent = 'Disconnected';
            staStatus.className = 'ip-value status-disconnected';
            staStatusBadge.textContent = 'Disconnected';
            staStatusBadge.className = 'badge status-disconnected';
        }
        
        // AP information
        document.getElementById('ap_ip').textContent = data.ap_ip || 'N/A';
        const clientCount = data.ap_connected_clients || 0;
        document.getElementById('ap_clients').textContent = `${clientCount} client(s)`;
        
        // Update connection buttons state
        this.updateConnectionButtons(data.sta_connecting, data.sta_connected);
    }

    // Update connection buttons based on state (same as before)
    updateConnectionButtons(connecting, connected) {
        const connectBtn = document.getElementById('connectStaBtn');
        const disconnectBtn = document.getElementById('disconnectStaBtn');
        const refreshBtn = document.querySelector('#network-status-heading ~ .ip-info-grid .btn-secondary');
        
        if (connectBtn && disconnectBtn) {
            if (connecting) {
                connectBtn.disabled = true;
                connectBtn.innerHTML = '<span class="btn-icon">⏳</span> Connecting...';
                disconnectBtn.disabled = true;
            } else if (connected) {
                connectBtn.disabled = true;
                connectBtn.innerHTML = '<span class="btn-icon">✅</span> Connected';
                disconnectBtn.disabled = false;
            } else {
                connectBtn.disabled = false;
                connectBtn.innerHTML = '<span class="btn-icon">🔌</span> Connect STA';
                disconnectBtn.disabled = true;
            }
        }
    }

    async refreshIPInfo() {
        StatusManager.showStatus('Refreshing all network status...', 'info');
        await this.loadIPInfo();
        StatusManager.showStatus('Network status updated', 'success');
    }

    // NEW: Connect to STA manually
    async connectSTA() {
        try {
            StatusManager.showStatus('Starting STA connection...', 'info');
            await ApiClient.post('/controlsta', { action: 'connect' });
            // Status will update automatically via auto-refresh
        } catch (error) {
            StatusManager.showStatus('Failed to start STA connection', 'error');
        }
    }

    // NEW: Disconnect STA manually
    async disconnectSTA() {
        try {
            StatusManager.showStatus('Disconnecting STA...', 'info');
            await ApiClient.post('/controlsta', { action: 'disconnect' });
            // Status will update automatically via auto-refresh
        } catch (error) {
            StatusManager.showStatus('Failed to disconnect STA', 'error');
        }
    }

    refreshUI() {
        // Refresh displays when WiFi tab becomes active
        this.loadIPInfo();
        console.log('WiFi tab UI refreshed');
    }
}

// Initialize immediately for SPA
window.wifiManager = new WifiManager();

// Refresh UI when WiFi tab becomes active
window.addEventListener('tabChanged', (e) => {
    if (e.detail.newTab === 'wifi') {
        window.wifiManager.refreshUI();
    }
});