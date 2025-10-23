class DebugConsole {
    constructor() {
        this.isEnabled = false;
        this.maxTableRows = 30; //live data table
        this.maxConsoleMessages = 30;  // Limit for raw MQTT console
        this.maxStoredMessages = 30; //For localStorage
        this.init();
    }

    init() {
        this.loadDebugState();
        this.bindEvents();
        this.startMessagePolling();
        this.loadStoredMessages();
    }

    bindEvents() {
        document.getElementById('debugEnabled').addEventListener('change', (e) => {
            this.toggleDebugMode(e.target.checked);
        });

        document.getElementById('clearConsole').addEventListener('click', () => {
            this.clearConsole();
        });

        document.getElementById('clearTable').addEventListener('click', () => {
            this.clearTable();
        });
    }

    async loadDebugState() {
        try {
            const response = await fetch('/getdebugstate');
            const data = await response.json();
            this.isEnabled = data.enabled;
            this.updateToggle();
            this.updateStatusMessages();
        } catch (error) {
            this.addMessage('Error loading debug state');
        }
    }

    async toggleDebugMode(enabled) {
        try {
            const response = await fetch('/toggledebug', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ enabled: enabled })
            });

            if (response.ok) {
                this.isEnabled = enabled;
                this.updateToggle();
                this.updateStatusMessages();
                this.addMessage(`Debug ${enabled ? 'ENABLED - MQTT + Console' : 'DISABLED - MQTT only'}`);
                
                if (enabled) {
                    this.loadStoredMessages();
                }
            }
        } catch (error) {
            this.addMessage('Error toggling debug mode');
        }
    }

    updateToggle() {
        const checkbox = document.getElementById('debugEnabled');
        const status = document.getElementById('debugStatus');
        
        if (checkbox) checkbox.checked = this.isEnabled;
        if (status) {
            status.textContent = this.isEnabled ? 'ON' : 'OFF';
            status.style.color = this.isEnabled ? '#10b981' : '#ef4444';
        }
    }

    parseMessageDynamic(messageData) {
        try {
            const parsed = JSON.parse(messageData.message);
            
            // FLAT STRUCTURE: All data is at root level
            const result = {
                deviceName: parsed.name || 'Unknown',
                deviceId: parsed.id || 'N/A', 
                topic: messageData.topic,
                data: {},
                timestamp: messageData.receivedAt || new Date().toISOString()
            };

            // Extract all data fields (exclude metadata)
            for (const [key, value] of Object.entries(parsed)) {
                if (!['id', 'name', 'mqtt_topic', 'timestamp', 'type'].includes(key)) {
                    if (value !== null && value !== undefined) {
                        result.data[key] = value;
                    }
                }
            }

            return result;

        } catch (error) {
            return {
                deviceName: 'Parse Error',
                deviceId: 'N/A',
                topic: messageData.topic,
                data: { raw: messageData.message },
                timestamp: new Date().toISOString()
            };
        }
    }

    extractAllData(obj) {
        const data = {};
        
        for (const [key, value] of Object.entries(obj)) {
            if (['id', 'name', 'mqtt_topic', 'timestamp', 'type'].includes(key)) continue;
            
            if (value !== null && value !== undefined) {
                if (typeof value === 'object' && !Array.isArray(value)) {
                    // Handle nested objects (shouldn't happen with flat structure, but just in case)
                    Object.assign(data, this.extractAllData(value));
                } else if (Array.isArray(value)) {
                    data[key] = value.join(', ');
                } else {
                    data[key] = value;
                }
            }
        }
        
        return data;
    }

    formatDataForTable(parsedMessage) {
        const { data } = parsedMessage;
        let html = '<div class="data-cell">';

        if (data.error) {
            html += `<div class="error-message">
                        <strong>Error:</strong> ${data.error}
                    </div>`;
        } else if (Object.keys(data).length > 0) {
            html += '<div class="dynamic-values">';
            
            for (const [key, value] of Object.entries(data)) {
                if (value !== null && value !== undefined) {
                    const formattedValue = this.formatNumberValue(value);
                    const displayKey = this.formatKeyName(key);
                    
                    html += `<div class="value-row">
                        <span class="value-name">${displayKey}:</span>
                        <span class="value-data">${formattedValue}</span>
                    </div>`;
                }
            }
            html += '</div>';
        } else {
            html += '<div class="no-data-message">No readable data</div>';
        }

        html += '</div>';
        return html;
    }

    formatNumberValue(value) {
        if (typeof value === 'number') {
            const absValue = Math.abs(value);
            
            if (Number.isInteger(value)) {
                return value.toString();
            } else if (absValue >= 1000) {
                return value.toFixed(0);
            } else if (absValue >= 100) {
                return value.toFixed(1);
            } else if (absValue >= 10) {
                return value.toFixed(2);
            } else if (absValue >= 1) {
                return value.toFixed(3);
            } else if (absValue >= 0.1) {
                return value.toFixed(4);
            } else if (absValue >= 0.01) {
                return value.toFixed(5);
            } else {
                return value.toFixed(6);
            }
        }
        
        return String(value);
    }

    formatKeyName(key) {
        // Convert snake_case to Title Case with spaces
        return key
            .split('_')
            .map(word => word.charAt(0).toUpperCase() + word.slice(1).toLowerCase())
            .join(' ');
    }

    addTableRow(messageData, saveToStorage = true) {
        const tableBody = document.getElementById('tableBody');
        if (!tableBody) return;

        if (tableBody.querySelector('.no-data')) {
            tableBody.innerHTML = '';
        }

        const parsedMessage = this.parseMessageDynamic(messageData);
        const { deviceName, deviceId, topic, timestamp } = parsedMessage;

        const displayTime = new Date(timestamp).toLocaleTimeString();

        const newRow = document.createElement('tr');
        newRow.className = 'new-row';
        newRow.innerHTML = `
            <td>${displayTime}</td>
            <td>${deviceName}</td>
            <td>${deviceId}</td>
            <td>${topic}</td>
            <td>${this.formatDataForTable(parsedMessage)}</td>
        `;

        tableBody.insertBefore(newRow, tableBody.firstChild);

        if (saveToStorage) {
            this.saveMessageToStorage(messageData);
        }

        while (tableBody.children.length > this.maxTableRows) {
            tableBody.removeChild(tableBody.lastChild);
        }
    }

    saveMessageToStorage(messageData) {
        try {
            const currentMessages = this.getStoredMessages();
            const timestampedMessage = {
                ...messageData,
                receivedAt: new Date().toISOString()
            };
            
            currentMessages.push(timestampedMessage);
            const toSave = currentMessages.slice(-this.maxStoredMessages);
            localStorage.setItem('mqttDebugMessages', JSON.stringify(toSave));
        } catch (error) {
            console.log('Storage save error');
        }
    }

    getStoredMessages() {
        try {
            const stored = localStorage.getItem('mqttDebugMessages');
            return stored ? JSON.parse(stored) : [];
        } catch (error) {
            return [];
        }
    }

    loadStoredMessages() {
        if (!this.isEnabled) return;
        
        try {
            const stored = localStorage.getItem('mqttDebugMessages');
            if (stored) {
                const messages = JSON.parse(stored);
                
                this.clearTable(true);
                
                messages.forEach(msg => {
                    this.addTableRow(msg, false);
                });
            }
        } catch (error) {
            console.log('No stored messages');
        }
    }

    addMessage(message) {
        if (!this.isEnabled) return;

        const consoleElement = document.getElementById('debugConsole');
        if (!consoleElement) return;

        // Clear "waiting for messages" placeholder
        if (consoleElement.children.length === 1 && 
            consoleElement.textContent.includes('MQTT Debug Mode is OFF')) {
            consoleElement.innerHTML = '';
        }

        // Add new message
        const messageElement = document.createElement('div');
        messageElement.className = 'console-line';
        messageElement.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
        consoleElement.appendChild(messageElement);

        // ENFORCE CONSOLE LIMIT (missing in current code)
        while (consoleElement.children.length > this.maxConsoleMessages) {
            consoleElement.removeChild(consoleElement.firstChild);
        }

        consoleElement.scrollTop = consoleElement.scrollHeight;
    }

    clearConsole() {
        const consoleElement = document.getElementById('debugConsole');
        if (consoleElement) {
            consoleElement.innerHTML = '';
            this.updateStatusMessages();
        }
    }

    clearTable(silent = false) {
        const tableBody = document.getElementById('tableBody');
        if (tableBody) {
            tableBody.innerHTML = '';
            localStorage.removeItem('mqttDebugMessages');
            if (!silent) {
                this.updateStatusMessages();
            }
        }
    }

    updateStatusMessages() {
        const tableBody = document.getElementById('tableBody');
        const consoleElement = document.getElementById('debugConsole');
        
        if (tableBody && tableBody.children.length === 0) {
            tableBody.innerHTML = '<tr><td colspan="5" class="no-data">' + 
                (this.isEnabled ? 'Waiting for data...' : 'MQTT Debug Mode is OFF. Enable to see data.') + 
                '</td></tr>';
        }

        if (consoleElement && consoleElement.children.length === 0) {
            consoleElement.innerHTML = '<div class="console-line">' + 
                (this.isEnabled ? 'Waiting for messages...' : 'MQTT Debug Mode is OFF. Enable to see messages.') + 
                '</div>';
        }
    }

    async checkForMessages() {
        if (!this.isEnabled) return;

        try {
            const response = await fetch('/getdebugmessages');
            const messages = await response.json();
            
            messages.forEach(msg => {
                const currentTime = new Date().toLocaleTimeString();
                this.addMessage(`MQTT [${msg.topic}]: ${msg.message}`);
                this.addTableRow(msg);
            });
        } catch (error) {
            // No new messages - silent fail
        }
    }

    startMessagePolling() {
        setInterval(() => this.checkForMessages(), 1000);
    }
}

document.addEventListener('DOMContentLoaded', () => {
    new DebugConsole();
});