class DebugConsole {
    constructor() {
        this.isEnabled = false;
        this.maxTableRows = 30;
        this.messageSequence = [];
        this.rawMessageSequence = [];
        this.deviceLastSeen = {};
        this.batchCount = 0;
        this.init();
    }

    init() {
        this.loadDebugState();
        this.bindEvents();
        this.startMessagePolling();
        this.loadStoredMessages();
    }

    bindEvents() {
        FormHelper.getElement('debugEnabled')?.addEventListener('change', 
            (e) => this.toggleDebugMode(e.target.checked));
        FormHelper.getElement('clearTable')?.addEventListener('click', 
            () => this.clearTable());
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
            
            // Check if this is a batch separator
            if (parsed.type === "batch_separator") {
                return {
                    isSeparator: true,
                    message: parsed.message || "Query Loop Completed",
                    realTime: messageData.realTime || this.getCurrentTime()
                };
            }
            
            // Regular message processing
            const result = {
                deviceName: parsed.name || 'Unknown',
                deviceId: parsed.id || 'N/A', 
                topic: messageData.topic,
                data: {},
                realTime: messageData.realTime || this.getCurrentTime(),
                sincePrev: messageData.timeDelta || "+0ms",
                sinceSame: messageData.sameDeviceDelta || "+0ms",
                espTimestamp: parsed.timestamp || Date.now(),
                browserTimestamp: Date.now(),
                isSeparator: false
            };

            // Extract data fields (exclude metadata)
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
                browserTimestamp: Date.now(),
                isSeparator: false
            };
        }
    }

    getCurrentTime() {
        return new Date().toTimeString().split(' ')[0]; // HH:MM:SS
    }

    addTableRow(messageData) {
        const tableBody = FormHelper.getElement('tableBody');
        const rawTableBody = FormHelper.getElement('rawTableBody');
        
        if (!tableBody || !rawTableBody) return;

        if (tableBody.querySelector('.no-data')) {
            tableBody.innerHTML = '';
        }
        if (rawTableBody.querySelector('.no-data')) {
            rawTableBody.innerHTML = '';
        }

        const parsedMessage = this.parseMessageDynamic(messageData);
        const rawMessage = this.createRawMessage(messageData);
        
        this.messageSequence.unshift(parsedMessage);
        this.rawMessageSequence.unshift(rawMessage);
        
        // Enforce message limit
        if (this.messageSequence.length > this.maxTableRows) {
            this.messageSequence.pop();
        }
        if (this.rawMessageSequence.length > this.maxTableRows) {
            this.rawMessageSequence.pop();
        }
        
        this.renderTable();
        this.renderRawTable();
        this.saveMessageToStorage(parsedMessage, rawMessage);
    }

    createRawMessage(messageData) {
        try {
            const parsed = JSON.parse(messageData.message);
            
            if (parsed.type === "batch_separator") {
                return {
                    isSeparator: true,
                    message: parsed.message || "Query Loop Completed",
                    realTime: messageData.realTime || this.getCurrentTime(),
                    rawJson: messageData.message
                };
            }
            
            return {
                deviceName: parsed.name || 'Unknown',
                deviceId: parsed.id || 'N/A',
                topic: messageData.topic,
                rawJson: messageData.message,
                realTime: messageData.realTime || this.getCurrentTime(),
                sincePrev: messageData.timeDelta || "+0ms",
                sinceSame: messageData.sameDeviceDelta || "+0ms",
                espTimestamp: parsed.timestamp || Date.now(),
                browserTimestamp: Date.now(),
                isSeparator: false
            };
        } catch (error) {
            return {
                deviceName: 'Parse Error',
                deviceId: 'N/A',
                topic: messageData.topic,
                rawJson: messageData.message,
                realTime: messageData.realTime || this.getCurrentTime(),
                sincePrev: messageData.timeDelta || "+0ms",
                sinceSame: messageData.sameDeviceDelta || "+0ms",
                espTimestamp: Date.now(),
                browserTimestamp: Date.now(),
                isSeparator: false
            };
        }
    }

    // ========== STORAGE MANAGEMENT ==========

    saveMessageToStorage(parsedMessage, rawMessage) {
        try {
            const currentMessages = this.getStoredMessages();
            const currentRawMessages = this.getStoredRawMessages();
            
            const storedMessage = this.createStoredMessage(parsedMessage);
            const storedRawMessage = this.createStoredRawMessage(rawMessage);
            
            currentMessages.unshift(storedMessage);
            currentRawMessages.unshift(storedRawMessage);
            
            const toSave = currentMessages.slice(0, this.maxTableRows);
            const toSaveRaw = currentRawMessages.slice(0, this.maxTableRows);
            
            localStorage.setItem('mqttDebugMessages', JSON.stringify(toSave));
            localStorage.setItem('mqttRawDebugMessages', JSON.stringify(toSaveRaw));
        } catch (error) {
            console.log('Storage save error');
        }
    }

    createStoredMessage(parsedMessage) {
        if (parsedMessage.isSeparator) {
            return {
                isSeparator: true,
                batchId: parsedMessage.batchId,
                batchDuration: parsedMessage.batchDuration,
                slaveCount: parsedMessage.slaveCount,
                pollInterval: parsedMessage.pollInterval,
                realTime: parsedMessage.realTime,
                message: parsedMessage.message,
                timestamp: parsedMessage.timestamp
            };
        }
        
        return {
            deviceName: parsedMessage.deviceName,
            deviceId: parsedMessage.deviceId,
            topic: parsedMessage.topic,
            data: parsedMessage.data,
            realTime: parsedMessage.realTime,
            sincePrev: parsedMessage.sincePrev,
            sinceSame: parsedMessage.sinceSame,
            espTimestamp: parsedMessage.espTimestamp,
            browserTimestamp: parsedMessage.browserTimestamp,
            isSeparator: false
        };
    }

    createStoredRawMessage(rawMessage) {
        if (rawMessage.isSeparator) {
            return {
                isSeparator: true,
                realTime: rawMessage.realTime,
                message: rawMessage.message,
                rawJson: rawMessage.rawJson,
                timestamp: rawMessage.timestamp
            };
        }
        
        return {
            deviceName: rawMessage.deviceName,
            deviceId: rawMessage.deviceId,
            topic: rawMessage.topic,
            rawJson: rawMessage.rawJson,
            realTime: rawMessage.realTime,
            sincePrev: rawMessage.sincePrev,
            sinceSame: rawMessage.sinceSame,
            espTimestamp: rawMessage.espTimestamp,
            browserTimestamp: rawMessage.browserTimestamp,
            isSeparator: false
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

    getStoredRawMessages() {
        try {
            const stored = localStorage.getItem('mqttRawDebugMessages');
            return stored ? JSON.parse(stored) : [];
        } catch {
            return [];
        }
    }

    loadStoredMessages() {
        try {
            const stored = localStorage.getItem('mqttDebugMessages');
            const storedRaw = localStorage.getItem('mqttRawDebugMessages');
            
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
                    espTimestamp: msg.espTimestamp || Date.now(),
                    isSeparator: msg.isSeparator || false,
                    message: msg.message
                })).slice(0, this.maxTableRows);
                
                this.renderTable();
            }
            
            if (storedRaw) {
                const rawMessages = JSON.parse(storedRaw);
                const rawTableBody = FormHelper.getElement('rawTableBody');
                
                if (rawTableBody?.querySelector('.no-data')) {
                    rawTableBody.innerHTML = '';
                }
                
                this.rawMessageSequence = rawMessages.map(msg => ({
                    deviceName: msg.deviceName,
                    deviceId: msg.deviceId,
                    topic: msg.topic,
                    rawJson: msg.rawJson,
                    realTime: msg.realTime,
                    sincePrev: msg.sincePrev,
                    sinceSame: msg.sinceSame,
                    espTimestamp: msg.espTimestamp || Date.now(),
                    isSeparator: msg.isSeparator || false,
                    message: msg.message
                })).slice(0, this.maxTableRows);
                
                this.renderRawTable();
            }
        } catch (error) {
            console.log('No stored table messages');
        }
    }

    // ========== UI RENDERING ==========

    renderTable() {
        const tableBody = FormHelper.getElement('tableBody');
        if (!tableBody) return;

        tableBody.innerHTML = this.messageSequence.map(item => {
            if (item.isSeparator) {
                return `
                    <tr class="batch-separator">
                        <td colspan="7">
                            ${item.message}
                        </td>
                    </tr>
                `;
            }

            const { realTime, sincePrev, sinceSame, deviceName, deviceId, topic } = item;
            return `
                <tr class="new-row">
                    <td>${realTime}</td>
                    <td>${sincePrev}</td>
                    <td>${sinceSame}</td>
                    <td>${deviceName}</td>
                    <td>${deviceId}</td>
                    <td>${topic}</td>
                    <td>${this.formatDataForTable(item)}</td>
                </tr>
            `;
        }).join('');
    }

    renderRawTable() {
        const rawTableBody = FormHelper.getElement('rawTableBody');
        if (!rawTableBody) return;

        rawTableBody.innerHTML = this.rawMessageSequence.map(item => {
            if (item.isSeparator) {
                return `
                    <tr class="batch-separator">
                        <td colspan="7">
                            ${item.message}
                        </td>
                    </tr>
                `;
            }

            const { realTime, sincePrev, sinceSame, deviceName, deviceId, topic } = item;
            return `
                <tr class="new-row">
                    <td>${realTime}</td>
                    <td>${sincePrev}</td>
                    <td>${sinceSame}</td>
                    <td>${deviceName}</td>
                    <td>${deviceId}</td>
                    <td>${topic}</td>
                    <td>${this.formatRawJson(item.rawJson)}</td>
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

    formatRawJson(rawJson) {
        try {
            const parsed = JSON.parse(rawJson);
            const formatted = JSON.stringify(parsed, null, 2);
            return `<div class="raw-json">${this.syntaxHighlight(formatted)}</div>`;
        } catch (error) {
            return `<div class="raw-json">${rawJson}</div>`;
        }
    }

    syntaxHighlight(json) {
        // Simple syntax highlighting for JSON
        return json.replace(/("(\\u[a-zA-Z0-9]{4}|\\[^u]|[^\\"])*"(\s*:)?|\b(true|false|null)\b|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?)/g, match => {
            let cls = 'raw-json-number';
            if (/^"/.test(match)) {
                if (/:$/.test(match)) {
                    cls = 'raw-json-key';
                } else {
                    cls = 'raw-json-string';
                }
            } else if (/true|false/.test(match)) {
                cls = 'raw-json-boolean';
            } else if (/null/.test(match)) {
                cls = 'raw-json-null';
            }
            return `<span class="${cls}">${match}</span>`;
        });
    }

    getValueTypeClass(key) {
        const keyLower = key.toLowerCase();
        const typeMap = {
            'volt': 'voltage',
            'current': 'current',
            'amp': 'current',
            'power': 'power',
            'watt': 'power',
            'energy': 'energy',
            'kwh': 'energy',
            'freq': 'frequency',
            'temp': 'temperature'
        };

        for (const [pattern, className] of Object.entries(typeMap)) {
            if (keyLower.includes(pattern)) return className;
        }
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
        const rawTableBody = FormHelper.getElement('rawTableBody');
        
        if (tableBody && rawTableBody) {
            tableBody.innerHTML = '';
            rawTableBody.innerHTML = '';
            localStorage.removeItem('mqttDebugMessages');
            localStorage.removeItem('mqttRawDebugMessages');
            this.messageSequence = [];
            this.rawMessageSequence = [];
            this.deviceLastSeen = {};
            this.batchCount = 0;
            this.updateStatusMessages();
            
            try {
                await ApiClient.post('/cleartable', {});
                StatusManager.showStatus('Both tables cleared and timing reset', 'success');
                console.log('✅ Both tables cleared and timing reset');
            } catch (error) {
                console.error('❌ Error resetting timing:', error);
                StatusManager.showStatus('Tables cleared (timing reset failed)', 'warning');
            }
        }
    }

    updateStatusMessages() {
        const statusText = this.isEnabled ? 'Waiting for data...' : 'MQTT Debug Mode is OFF. Enable to see data.';
        
        const tableBody = FormHelper.getElement('tableBody');
        const rawTableBody = FormHelper.getElement('rawTableBody');
        
        if (tableBody && !tableBody.children.length) {
            tableBody.innerHTML = `<tr><td colspan="7" class="no-data">${statusText}</td></tr>`;
        }
        if (rawTableBody && !rawTableBody.children.length) {
            rawTableBody.innerHTML = `<tr><td colspan="7" class="no-data">${statusText}</td></tr>`;
        }
    }

    // ========== POLLING ==========

    async checkForMessages() {
        if (!this.isEnabled) return;

        try {
            const messages = await ApiClient.get('/getdebugmessages');
            messages.forEach(msg => this.addTableRow(msg));
        } catch (error) {
            // Silent fail - no new messages
        }
    }

    startMessagePolling() {
        setInterval(() => this.checkForMessages(), 1000);
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    new DebugConsole();
});
