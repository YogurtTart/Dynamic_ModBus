class DebugConsole {
    constructor() {
        this.isEnabled = false;
        this.maxTableRows = 30;
        this.maxConsoleMessages = 30;
        this.messageSequence = []; // NEWEST first (top of table)
        this.init();
    }

    init() {
        this.loadDebugState();
        this.bindEvents();
        this.startMessagePolling();
        this.loadStoredMessages();
        this.loadStoredConsoleMessages();
    }

    bindEvents() {
        const elements = {
            'debugEnabled': (e) => this.toggleDebugMode(e.target.checked),
            'clearConsole': () => this.clearConsole(),
            'clearTable': () => this.clearTable()
        };

        Object.entries(elements).forEach(([id, handler]) => {
            FormHelper.getElement(id)?.addEventListener(
                id === 'debugEnabled' ? 'change' : 'click', 
                handler
            );
        });
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
            this.addMessage(`Debug ${enabled ? 'ENABLED' : 'DISABLED'}`);
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

    // ========== TIMING CALCULATIONS ==========
    calculateSequenceTiming() {
        if (this.messageSequence.length === 0) return;

        this.messageSequence[0].displayTime = "0s";

        for (let i = 1; i < this.messageSequence.length; i++) {
            const currentMsg = this.messageSequence[i];
            const aboveMsg = this.messageSequence[i-1];
            
            const currentTime = this.normalizeTimestamp(currentMsg.espTimestamp);
            const aboveTime = this.normalizeTimestamp(aboveMsg.espTimestamp);
            const delta = Math.max(0, aboveTime - currentTime);
            
            currentMsg.displayTime = this.formatSimpleDelta(delta);
        }
    }

    updateTimingForNewMessage(newMessage) {
        newMessage.displayTime = "0s";
        
        if (this.messageSequence.length > 1) {
            const belowMsg = this.messageSequence[1];
            const newTime = this.normalizeTimestamp(newMessage.espTimestamp);
            const belowTime = this.normalizeTimestamp(belowMsg.espTimestamp);
            const delta = Math.max(0, newTime - belowTime);
            belowMsg.displayTime = this.formatSimpleDelta(delta);
        }
    }

    normalizeTimestamp(timestamp) {
        if (!timestamp || timestamp === 0) return Date.now();
        
        if (timestamp > 1000000000 && timestamp < 2000000000) {
            return timestamp * 1000;
        }
        
        const now = Date.now();
        if (timestamp > now + 31536000000) return now;
        
        return timestamp;
    }

    formatSimpleDelta(millis) {
        if (millis === 0) return "0s";
        if (millis < 1000) return `+${millis}ms`;
        return `+${Math.floor(millis / 1000)}s`;
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
                espTimestamp: parsed.timestamp || Date.now(),
                displayTime: ""
            };

            // Extract data fields (exclude metadata)
            for (const [key, value] of Object.entries(parsed)) {
                if (!['id', 'name', 'mqtt_topic', 'timestamp', 'type'].includes(key) && value != null) {
                    result.data[key] = value;
                }
            }

            return result;
        } catch (error) {
            return {
                deviceName: 'Parse Error',
                deviceId: 'N/A',
                topic: messageData.topic,
                data: { raw: messageData.message },
                espTimestamp: Date.now(),
                displayTime: ""
            };
        }
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
        
        this.updateTimingForNewMessage(parsedMessage);
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

    saveFixedDataToStorage() {
        try {
            const fixedMessages = this.messageSequence.map(msg => 
                this.createStoredMessage(msg)
            );
            localStorage.setItem('mqttDebugMessages', JSON.stringify(fixedMessages));
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
            displayTime: parsedMessage.displayTime,
            espTimestamp: parsedMessage.espTimestamp,
            receivedAt: new Date().toISOString(),
            browserTimestamp: Date.now()
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
                    displayTime: msg.displayTime,
                    espTimestamp: msg.espTimestamp || Date.now()
                })).slice(0, this.maxTableRows);
                
                this.calculateSequenceTiming();
                this.saveFixedDataToStorage();
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
            const { deviceName, deviceId, topic, displayTime } = parsedMessage;
            return `
                <tr class="new-row">
                    <td>${displayTime}</td>
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
        if (data.error) return `<div class="error-message"><strong>Error:</strong> ${data.error}</div>`;

        const valuesHtml = Object.entries(data)
            .filter(([_, value]) => value != null)
            .map(([key, value]) => `
                <div class="value-row">
                    <span class="value-name">${this.formatKeyName(key)}:</span>
                    <span class="value-data">${this.formatNumberValue(value)}</span>
                </div>`
            ).join('');

        return `<div class="data-cell"><div class="dynamic-values">${valuesHtml}</div></div>`;
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

    // ========== CONSOLE MESSAGES ==========
    addMessage(message) {
        if (!this.isEnabled) return;

        const consoleElement = FormHelper.getElement('debugConsole');
        if (!consoleElement) return;

        // Clear waiting message if present
        if (consoleElement.children.length === 1 && 
            consoleElement.children[0]?.textContent?.includes('Waiting for messages')) {
            consoleElement.innerHTML = '';
        }

        const messageElement = document.createElement('div');
        messageElement.className = 'console-line';
        messageElement.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
        consoleElement.appendChild(messageElement);

        // Enforce message limit
        while (consoleElement.children.length > this.maxConsoleMessages) {
            consoleElement.removeChild(consoleElement.firstChild);
        }

        consoleElement.scrollTop = consoleElement.scrollHeight;
        this.saveConsoleMessage(message);
    }

    saveConsoleMessage(message) {
        try {
            const currentMessages = this.getStoredConsoleMessages();
            currentMessages.unshift({
                message,
                timestamp: new Date().toISOString()
            });
            
            const toSave = currentMessages.slice(0, this.maxConsoleMessages);
            localStorage.setItem('mqttConsoleMessages', JSON.stringify(toSave));
        } catch (error) {
            console.log('Console storage error');
        }
    }

    getStoredConsoleMessages() {
        try {
            const stored = localStorage.getItem('mqttConsoleMessages');
            return stored ? JSON.parse(stored) : [];
        } catch {
            return [];
        }
    }

    loadStoredConsoleMessages() {
        try {
            const stored = localStorage.getItem('mqttConsoleMessages');
            if (stored) {
                const messages = JSON.parse(stored);
                const consoleElement = FormHelper.getElement('debugConsole');
                if (consoleElement) {
                    consoleElement.innerHTML = messages.slice(0, this.maxConsoleMessages).map(msg => 
                        `<div class="console-line">[${new Date(msg.timestamp).toLocaleTimeString()}] ${msg.message}</div>`
                    ).join('');
                    consoleElement.scrollTop = consoleElement.scrollHeight;
                }
            }
        } catch (error) {
            console.log('No stored console messages');
        }
    }

    // ========== CLEANUP & STATUS ==========
    clearConsole() {
        const consoleElement = FormHelper.getElement('debugConsole');
        if (consoleElement) {
            consoleElement.innerHTML = '';
            localStorage.removeItem('mqttConsoleMessages');
            this.updateStatusMessages();
        }
    }

    clearTable() {
        const tableBody = FormHelper.getElement('tableBody');
        if (tableBody) {
            tableBody.innerHTML = '';
            localStorage.removeItem('mqttDebugMessages');
            this.messageSequence = [];
            this.updateStatusMessages();
        }
    }

    updateStatusMessages() {
        const statusText = this.isEnabled ? 'Waiting for data...' : 'MQTT Debug Mode is OFF. Enable to see data.';
        
        const tableBody = FormHelper.getElement('tableBody');
        if (tableBody && !tableBody.children.length) {
            tableBody.innerHTML = `<tr><td colspan="5" class="no-data">${statusText}</td></tr>`;
        }

        const consoleElement = FormHelper.getElement('debugConsole');
        if (consoleElement && !consoleElement.children.length) {
            consoleElement.innerHTML = `<div class="console-line">${statusText}</div>`;
        }
    }

    // ========== POLLING ==========
    async checkForMessages() {
        if (!this.isEnabled) return;

        try {
            const messages = await ApiClient.get('/getdebugmessages');
            messages.forEach(msg => {
                this.addMessage(`MQTT [${msg.topic}]: ${msg.message}`);
                this.addTableRow(msg);
            });
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