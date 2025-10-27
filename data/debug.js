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
        FormHelper.getElement('debugEnabled')?.addEventListener('change', (e) => {
            this.toggleDebugMode(e.target.checked);
        });

        FormHelper.getElement('clearConsole')?.addEventListener('click', () => {
            this.clearConsole();
        });

        FormHelper.getElement('clearTable')?.addEventListener('click', () => {
            this.clearTable();
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

    // FIXED: Rolling consecutive timing - newest always 0s, others show difference from above
    calculateSequenceTiming() {
        if (this.messageSequence.length === 0) return;

        // Always set the newest message (top) to "0s"
        this.messageSequence[0].displayTime = "0s";

        // For each subsequent message, calculate time difference from the message ABOVE it
        for (let i = 1; i < this.messageSequence.length; i++) {
            const currentMsg = this.messageSequence[i];      // Current message (older)
            const aboveMsg = this.messageSequence[i-1];      // Message above it (newer)
            
            // Get timestamps in milliseconds
            const currentTime = this.normalizeTimestamp(currentMsg.espTimestamp);
            const aboveTime = this.normalizeTimestamp(aboveMsg.espTimestamp);
            
            // Difference = above message time - current message time
            const delta = Math.max(0, aboveTime - currentTime);
            currentMsg.displayTime = this.formatSimpleDelta(delta);
        }
    }

    // FIXED: Better timestamp normalization
    normalizeTimestamp(timestamp) {
        if (!timestamp || timestamp === 0) return Date.now();
        
        // If timestamp is unreasonable (like 1.7 billion), treat as seconds and convert
        if (timestamp > 1000000000 && timestamp < 2000000000) {
            return timestamp * 1000; // Convert seconds to milliseconds
        }
        
        // If timestamp is in future or unreasonable, use current time
        const now = Date.now();
        if (timestamp > now + 31536000000) { // More than 1 year in future
            return now;
        }
        
        return timestamp;
    }

    // Simple formatting
    formatSimpleDelta(millis) {
        if (millis === 0) return "0s";
        
        if (millis < 1000) {
            return `+${millis}ms`;
        } else {
            const seconds = Math.floor(millis / 1000);
            return `+${seconds}s`;
        }
    }

    renderTable() {
        const tableBody = FormHelper.getElement('tableBody');
        if (!tableBody) return;

        tableBody.innerHTML = '';

        // Display in current order (newest first at top)
        this.messageSequence.forEach(parsedMessage => {
            const { deviceName, deviceId, topic, displayTime } = parsedMessage;

            const newRow = document.createElement('tr');
            newRow.className = 'new-row';
            newRow.innerHTML = `
                <td>${displayTime}</td>
                <td>${deviceName}</td>
                <td>${deviceId}</td>
                <td>${topic}</td>
                <td>${this.formatDataForTable(parsedMessage)}</td>
            `;
            tableBody.appendChild(newRow);
        });
    }

    // FIXED: Better timestamp parsing
    parseMessageDynamic(messageData) {
        try {
            const parsed = JSON.parse(messageData.message);
            
            // Use current time if no timestamp or invalid timestamp
            let espTimestamp = parsed.timestamp;
            if (!espTimestamp || espTimestamp === 0) {
                espTimestamp = Date.now();
            }

            const result = {
                deviceName: parsed.name || 'Unknown',
                deviceId: parsed.id || 'N/A', 
                topic: messageData.topic,
                data: {},
                espTimestamp, // Store as-is, will normalize in timing calculation
                displayTime: "0s"
            };

            // Extract data fields
            for (const [key, value] of Object.entries(parsed)) {
                if (!['id', 'name', 'mqtt_topic', 'timestamp', 'type'].includes(key) && value != null) {
                    result.data[key] = value;
                }
            }

            return result;
        } catch (error) {
            // For parse errors, use current time
            return {
                deviceName: 'Parse Error',
                deviceId: 'N/A',
                topic: messageData.topic,
                data: { raw: messageData.message },
                espTimestamp: Date.now(),
                displayTime: "0s"
            };
        }
    }

    formatDataForTable(parsedMessage) {
        const { data } = parsedMessage;
        if (!Object.keys(data).length) return '<div class="no-data-message">No readable data</div>';

        if (data.error) {
            return `<div class="error-message"><strong>Error:</strong> ${data.error}</div>`;
        }

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

    // FIXED: Properly maintain 30 messages with stable timing
    addTableRow(messageData) {
        const tableBody = FormHelper.getElement('tableBody');
        if (!tableBody) return;

        if (tableBody.querySelector('.no-data')) {
            tableBody.innerHTML = '';
        }

        const parsedMessage = this.parseMessageDynamic(messageData);
        
        // Add new message to the BEGINNING (newest first at top)
        this.messageSequence.unshift(parsedMessage);
        
        // STRICTLY enforce 30 message limit by removing OLDEST (last in array)
        if (this.messageSequence.length > this.maxTableRows) {
            this.messageSequence.pop(); // Remove oldest (last element)
        }
        
        // Recalculate consecutive timing
        this.calculateSequenceTiming();
        
        this.renderTable();
        this.saveMessageToStorage(messageData, parsedMessage.espTimestamp);
    }

    saveMessageToStorage(messageData, espTimestamp) {
        try {
            const currentMessages = this.getStoredMessages();
            const timestampedMessage = {
                ...messageData,
                receivedAt: new Date().toISOString(),
                espTimestamp,
                browserTimestamp: Date.now()
            };
            
            currentMessages.unshift(timestampedMessage);
            
            // STRICTLY enforce 30 message limit in storage too
            const toSave = currentMessages.slice(0, this.maxTableRows);
            localStorage.setItem('mqttDebugMessages', JSON.stringify(toSave));
        } catch (error) {
            console.log('Storage save error');
        }
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
                
                // Load messages and enforce 30 limit
                this.messageSequence = messages.map(msg => this.parseMessageDynamic(msg));
                this.messageSequence = this.messageSequence.slice(0, this.maxTableRows);
                
                this.calculateSequenceTiming();
                this.renderTable();
            }
        } catch (error) {
            console.log('No stored table messages');
        }
    }

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

        // STRICTLY enforce 30 message limit
        while (consoleElement.children.length > this.maxConsoleMessages) {
            consoleElement.removeChild(consoleElement.firstChild);
        }

        consoleElement.scrollTop = consoleElement.scrollHeight;
        this.saveConsoleMessageToStorage(message);
    }

    saveConsoleMessageToStorage(message) {
        try {
            const currentMessages = this.getStoredConsoleMessages();
            currentMessages.unshift({
                message,
                timestamp: new Date().toISOString()
            });
            
            // STRICTLY enforce 30 message limit in storage
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
                    consoleElement.innerHTML = '';
                    
                    // Load and enforce 30 message limit
                    const messagesToShow = messages.slice(0, this.maxConsoleMessages);
                    messagesToShow.forEach(msg => {
                        const messageElement = document.createElement('div');
                        messageElement.className = 'console-line';
                        messageElement.textContent = `[${new Date(msg.timestamp).toLocaleTimeString()}] ${msg.message}`;
                        consoleElement.appendChild(messageElement);
                    });
                    
                    consoleElement.scrollTop = consoleElement.scrollHeight;
                }
            }
        } catch (error) {
            console.log('No stored console messages');
        }
    }

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
        const tableBody = FormHelper.getElement('tableBody');
        const consoleElement = FormHelper.getElement('debugConsole');
        const statusText = this.isEnabled ? 'Waiting for data...' : 'MQTT Debug Mode is OFF. Enable to see data.';
        
        if (tableBody && !tableBody.children.length) {
            tableBody.innerHTML = `<tr><td colspan="5" class="no-data">${statusText}</td></tr>`;
        }

        if (consoleElement && !consoleElement.children.length) {
            consoleElement.innerHTML = `<div class="console-line">${statusText}</div>`;
        }
    }

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

document.addEventListener('DOMContentLoaded', () => {
    new DebugConsole();
});