class DebugConsole {
    constructor() {
        this.isEnabled = false;
        this.maxTableRows = 30;
        this.messageSequence = [];
        this.deviceLastSeen = {}; // Track last seen time per device
        this.init();
    }

    init() {
        this.loadDebugState();
        this.bindEvents();
        this.startMessagePolling();
        this.loadStoredMessages();
    }

    bindEvents() {
        const debugToggle = FormHelper.getElement('debugEnabled');
        const clearButton = FormHelper.getElement('clearTable');
        const resetTimingButton = FormHelper.getElement('resetTiming'); // Add this button
        
        if (debugToggle) {
            debugToggle.addEventListener('change', (e) => this.toggleDebugMode(e.target.checked));
        }
        
        if (clearButton) {
            clearButton.addEventListener('click', () => this.clearTable());
        }
        
        if (resetTimingButton) {
            resetTimingButton.addEventListener('click', () => this.resetTiming());
        }
    }

    async loadDebugState() {
        try {
            const data = await ApiClient.get('/getdebugstate');
            this.isEnabled = data.enabled;
            this.updateToggle();
            this.updateStatusMessages();
        } catch (error) {
            StatusManager.showStatus('Error loading debug state', 'error');
        }
    }

    async toggleDebugMode(enabled) {
        try {
            await ApiClient.post('/toggledebug', { enabled });
            this.isEnabled = enabled;
            this.updateToggle();
            this.updateStatusMessages();
        } catch (error) {
            StatusManager.showStatus('Error toggling debug mode', 'error');
        }
    }

    updateToggle() {
        const checkbox = FormHelper.getElement('debugEnabled');
        const status = FormHelper.getElement('debugStatus');
        
        if (checkbox) checkbox.checked = this.isEnabled;
        if (status) {
            status.textContent = this.isEnabled ? 'ON' : 'OFF';
            status.style.color = this.isEnabled ? '#10b981' : '#ef4444';
        }
    }

    // ========== MESSAGE PROCESSING ==========
    
    parseMessageDynamic(messageData) {
        try {
            const parsed = JSON.parse(messageData.message);
            
            const result = {
                deviceName: parsed.name || 'Unknown',
                deviceId: parsed.id || 'N/A', 
                topic: messageData.topic,
                data: {},
                realTime: messageData.realTime || this.getCurrentTime(),
                sincePrev: messageData.timeDelta || "+0ms",
                sinceSame: messageData.sameDeviceDelta || "+0ms",
                espTimestamp: parsed.timestamp || Date.now(),
                browserTimestamp: Date.now()
            };

            // Extract data fields
            for (const key in parsed) {
                if (!['id', 'name', 'mqtt_topic', 'timestamp', 'type', 'timeDelta'].includes(key) && parsed[key] != null) {
                    result.data[key] = parsed[key];
                }
            }

            return result;
        } catch (error) {
            return {
                deviceName: 'Parse Error',
                deviceId: 'N/A',
                topic: messageData.topic,
                data: { raw: messageData.message },
                realTime: messageData.realTime || this.getCurrentTime(),
                sincePrev: messageData.timeDelta || "+0ms",
                sinceSame: messageData.sameDeviceDelta || "+0ms",
                espTimestamp: Date.now(),
                browserTimestamp: Date.now()
            };
        }
    }

    getCurrentTime() {
        const now = new Date();
        return now.toTimeString().split(' ')[0]; // HH:MM:SS
    }

    addTableRow(messageData) {
        const tableBody = FormHelper.getElement('tableBody');
        if (!tableBody) return;

        if (tableBody.querySelector('.no-data')) {
            tableBody.innerHTML = '';
        }

        const parsedMessage = this.parseMessageDynamic(messageData);
        this.messageSequence.unshift(parsedMessage);
        
        // Enforce message limit
        if (this.messageSequence.length > this.maxTableRows) {
            this.messageSequence.pop();
        }
        
        this.renderTable();
        this.saveMessageToStorage(parsedMessage);
    }

    // ========== STORAGE MANAGEMENT ==========

    saveMessageToStorage(parsedMessage) {
        try {
            const currentMessages = this.getStoredMessages();
            const storedMessage = this.createStoredMessage(parsedMessage);
            
            currentMessages.unshift(storedMessage);
            const toSave = currentMessages.slice(0, this.maxTableRows);
            localStorage.setItem('mqttDebugMessages', JSON.stringify(toSave));
        } catch (error) {
            console.log('Storage save error');
        }
    }

    createStoredMessage(parsedMessage) {
        return {
            deviceName: parsedMessage.deviceName,
            deviceId: parsedMessage.deviceId,
            topic: parsedMessage.topic,
            data: parsedMessage.data,
            realTime: parsedMessage.realTime,
            sincePrev: parsedMessage.sincePrev,
            sinceSame: parsedMessage.sinceSame,
            espTimestamp: parsedMessage.espTimestamp,
            browserTimestamp: parsedMessage.browserTimestamp
        };
    }

    getStoredMessages() {
        try {
            const stored = localStorage.getItem('mqttDebugMessages');
            return stored ? JSON.parse(stored) : [];
        } catch {
            return [];
        }
    }

    loadStoredMessages() {
        try {
            const stored = localStorage.getItem('mqttDebugMessages');
            if (stored) {
                const messages = JSON.parse(stored);
                const tableBody = FormHelper.getElement('tableBody');
                
                if (tableBody?.querySelector('.no-data')) {
                    tableBody.innerHTML = '';
                }
                
                this.messageSequence = messages.map(msg => ({
                    deviceName: msg.deviceName,
                    deviceId: msg.deviceId,
                    topic: msg.topic,
                    data: msg.data,
                    realTime: msg.realTime,
                    sincePrev: msg.sincePrev,
                    sinceSame: msg.sinceSame,
                    espTimestamp: msg.espTimestamp || Date.now()
                })).slice(0, this.maxTableRows);
                
                this.renderTable();
            }
        } catch (error) {
            console.log('No stored table messages');
        }
    }

    // ========== UI RENDERING ==========

    renderTable() {
        const tableBody = FormHelper.getElement('tableBody');
        if (!tableBody) return;

        tableBody.innerHTML = this.messageSequence.map(parsedMessage => {
            const { realTime, sincePrev, sinceSame, deviceName, deviceId, topic } = parsedMessage;
            return `
                <tr class="new-row">
                    <td>${realTime}</td>
                    <td>${sincePrev}</td>
                    <td>${sinceSame}</td>
                    <td>${deviceName}</td>
                    <td>${deviceId}</td>
                    <td>${topic}</td>
                    <td>${this.formatDataForTable(parsedMessage)}</td>
                </tr>
            `;
        }).join('');
    }

    formatDataForTable(parsedMessage) {
        const { data } = parsedMessage;
        if (!Object.keys(data).length) return '<div class="no-data-message">No readable data</div>';
        if (data.error) return `<div class="compact-error"><strong>Error:</strong> ${data.error}</div>`;

        const dataItems = Object.entries(data)
            .filter(([_, value]) => value != null)
            .map(([key, value]) => {
                const formattedValue = this.formatNumberValue(value);
                const valueClass = this.getValueTypeClass(key);
                return `
                    <div class="data-item">
                        <div class="data-label">${this.formatKeyName(key)}</div>
                        <div class="data-value ${valueClass}">${formattedValue}</div>
                    </div>
                `;
            }).join('');

        return `<div class="data-cell"><div class="data-grid">${dataItems}</div></div>`;
    }

    getValueTypeClass(key) {
        const keyLower = key.toLowerCase();
        if (keyLower.includes('volt') || keyLower.includes('voltage')) return 'voltage';
        if (keyLower.includes('current') || keyLower.includes('amp')) return 'current';
        if (keyLower.includes('power') || keyLower.includes('watt')) return 'power';
        if (keyLower.includes('energy') || keyLower.includes('kwh')) return 'energy';
        if (keyLower.includes('freq')) return 'frequency';
        if (keyLower.includes('temp')) return 'temperature';
        return '';
    }

    formatNumberValue(value) {
        if (typeof value !== 'number') return String(value);
        if (Number.isInteger(value)) return value.toString();
        
        const absValue = Math.abs(value);
        if (absValue >= 1000) return value.toFixed(0);
        if (absValue >= 100) return value.toFixed(1);
        if (absValue >= 10) return value.toFixed(2);
        if (absValue >= 1) return value.toFixed(3);
        if (absValue >= 0.1) return value.toFixed(4);
        if (absValue >= 0.01) return value.toFixed(5);
        return value.toFixed(6);
    }

    formatKeyName(key) {
        return key.split('_')
            .map(word => word.charAt(0).toUpperCase() + word.slice(1).toLowerCase())
            .join(' ');
    }

    // ========== CLEANUP & STATUS ==========

    async clearTable() {
        const tableBody = FormHelper.getElement('tableBody');
        if (tableBody) {
            // Clear frontend table immediately
            tableBody.innerHTML = '';
            localStorage.removeItem('mqttDebugMessages');
            this.messageSequence = [];
            this.deviceLastSeen = {};
            this.updateStatusMessages();
            
            // Reset backend timing data (all 3 time columns)
            try {
                await ApiClient.post('/cleartable', {});
                StatusManager.showStatus('Table cleared and timing reset', 'success');
                console.log('✅ Table cleared and timing reset - Real Time, Since Prev, Since Same');
            } catch (error) {
                console.error('❌ Error resetting timing:', error);
                StatusManager.showStatus('Table cleared (timing reset failed)', 'warning');
            }
        }
    }

    updateStatusMessages() {
        const statusText = this.isEnabled ? 'Waiting for data...' : 'MQTT Debug Mode is OFF. Enable to see data.';
        
        const tableBody = FormHelper.getElement('tableBody');
        if (tableBody && !tableBody.children.length) {
            tableBody.innerHTML = `<tr><td colspan="7" class="no-data">${statusText}</td></tr>`;
        }
    }

    // ========== POLLING ==========

    async checkForMessages() {
        if (!this.isEnabled) return;

        try {
            const messages = await ApiClient.get('/getdebugmessages');
            messages.forEach(msg => {
                this.addTableRow(msg);
            });
        } catch (error) {
            // Silent fail - no new messages
        }
    }

    startMessagePolling() {
        setInterval(() => this.checkForMessages(), 1000);
    }

    async resetTiming() {
        try {
            await ApiClient.post('/resettiming', {});
            this.clearTable(); // Also clear the table when resetting timing
            StatusManager.showStatus('Timing data reset', 'success');
        } catch (error) {
            StatusManager.showStatus('Error resetting timing', 'error');
        }
    }
}



// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    new DebugConsole();
});