// script.js - Optimized WiFi configuration page
class WifiManager {
    constructor() {
        this.init();
    }

    init() {
        this.bindEvents();
        this.loadInitialData();
    }

    bindEvents() {
        // Enter key support for forms
        document.addEventListener('keypress', (e) => {
            if (e.key === 'Enter' && e.target.closest('form')) {
                e.preventDefault();
                if (e.target.closest('#wifiForm')) {
                    this.saveSettings();
                }
            }
        });
    }

    async loadInitialData() {
        try {
            await Promise.all([
                this.loadSettings(),
                this.loadIPInfo()
            ]);
            
            // Auto-refresh IP info every 30 seconds
            setInterval(() => this.loadIPInfo(), 30000);
        } catch (error) {
            console.error('Initialization error:', error);
        }
    }

    async loadSettings() {
        try {
            const data = await ApiClient.get('/getwifi');
            
            FormHelper.setValue('sta_ssid', data.sta_ssid);
            FormHelper.setValue('sta_password', data.sta_password);
            FormHelper.setValue('ap_ssid', data.ap_ssid);
            FormHelper.setValue('ap_password', data.ap_password);
            FormHelper.setValue('mqtt_server', data.mqtt_server);
            FormHelper.setValue('mqtt_port', data.mqtt_port);
            
            StatusManager.showStatus('WiFi & MQTT settings loaded successfully!', 'success');
        } catch (error) {
            // Error handled by ApiClient
        }
    }

    async saveSettings() {
        const requiredFields = ['sta_ssid', 'sta_password', 'ap_ssid', 'ap_password', 'mqtt_server', 'mqtt_port'];
        if (!FormHelper.validateRequired(requiredFields)) return;

        const formData = new FormData();
        formData.append('sta_ssid', FormHelper.getValue('sta_ssid'));
        formData.append('sta_password', FormHelper.getValue('sta_password'));
        formData.append('ap_ssid', FormHelper.getValue('ap_ssid'));
        formData.append('ap_password', FormHelper.getValue('ap_password'));
        formData.append('mqtt_server', FormHelper.getValue('mqtt_server'));
        formData.append('mqtt_port', FormHelper.getValue('mqtt_port'));

        try {
            await ApiClient.postForm('/savewifi', formData);
            StatusManager.showStatus('WiFi & MQTT settings saved successfully! Device will use new settings on restart.', 'success');
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
        
        // AP information
        document.getElementById('ap_ip').textContent = data.ap_ip;
        document.getElementById('ap_clients').textContent = 
            data.ap_connected_clients + ' client(s)';
    }

    async refreshIPInfo() {
        StatusManager.showStatus('Refreshing network status...', 'info');
        await this.loadIPInfo();
        StatusManager.showStatus('Network status updated', 'success');
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

