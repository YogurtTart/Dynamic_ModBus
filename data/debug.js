class DebugConsole {
    constructor() {
        this.isEnabled = false;
        this.maxTableRows = 30;
        this.messageSequence = [];
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
    
    createMessageObject(messageData) {
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

    getCurrentTime() {
        return new Date().toTimeString().split(' ')[0];
    }

    addTableRow(messageData) {
        const tableBody = FormHelper.getElement('tableBody');
        if (!tableBody) return;

        if (tableBody.querySelector('.no-data')) {
            tableBody.innerHTML = '';
        }

        const messageObject = this.createMessageObject(messageData);
        
        this.messageSequence.unshift(messageObject);
        
        if (this.messageSequence.length > this.maxTableRows) {
            this.messageSequence.pop();
        }
        
        this.addSingleRowToTable(messageObject);
        this.saveMessageToStorage(messageObject);
    }

    // ========== SMART RENDERING ==========

    // REPLACE the existing addSingleRowToTable method with this:

    addSingleRowToTable(messageObject) {
        const tableBody = FormHelper.getElement('tableBody');
        if (!tableBody) return;

        // Store scroll state before update
        const tableContainer = tableBody.closest('.table-container');
        const previousScrollTop = tableContainer ? tableContainer.scrollTop : 0;
        const wasScrolled = previousScrollTop > 0;

        const newRowHtml = this.createRowHtml(messageObject);
        
        if (tableBody.firstChild) {
            tableBody.insertAdjacentHTML('afterbegin', newRowHtml);
        } else {
            tableBody.innerHTML = newRowHtml;
        }
        
        this.trimExcessRows();
        
        // Maintain scroll position for users who are scrolled down
        if (tableContainer && wasScrolled) {
            // Estimate new row height and adjust scroll position
            const estimatedRowHeight = 60; // pixels
            tableContainer.scrollTop = previousScrollTop + estimatedRowHeight;
        }
    }

    createRowHtml(messageObject) {
        if (messageObject.isSeparator) {
            return `
                <tr class="batch-separator">
                    <td colspan="7">${messageObject.message}</td>
                </tr>
            `;
        }

        const { realTime, sincePrev, sinceSame, deviceName, deviceId, topic } = messageObject;
        return `
            <tr class="new-row">
                <td>${realTime}</td>
                <td>${sincePrev}</td>
                <td>${sinceSame}</td>
                <td>${deviceName}</td>
                <td>${deviceId}</td>
                <td>${topic}</td>
                <td>${this.formatRawJson(messageObject.rawJson)}</td>
            </tr>
        `;
    }

    trimExcessRows() {
        const tableBody = FormHelper.getElement('tableBody');
        if (!tableBody) return;

        const allRows = tableBody.querySelectorAll('tr');
        
        if (allRows.length > this.maxTableRows) {
            const rowsToRemove = allRows.length - this.maxTableRows;
            
            for (let i = 0; i < rowsToRemove; i++) {
                allRows[allRows.length - 1 - i].remove();
            }
        }
    }

    // ========== STORAGE MANAGEMENT ==========

    saveMessageToStorage(messageObject) {
        try {
            const currentMessages = this.getStoredMessages();
            const storedMessage = this.createStoredMessage(messageObject);
            
            currentMessages.unshift(storedMessage);
            const toSave = currentMessages.slice(0, this.maxTableRows);
            
            localStorage.setItem('mqttDebugMessages', JSON.stringify(toSave));
        } catch (error) {
            console.log('Storage save error');
        }
    }

    createStoredMessage(messageObject) {
        if (messageObject.isSeparator) {
            return {
                isSeparator: true,
                realTime: messageObject.realTime,
                message: messageObject.message,
                rawJson: messageObject.rawJson,
                timestamp: messageObject.timestamp
            };
        }
        
        return {
            deviceName: messageObject.deviceName,
            deviceId: messageObject.deviceId,
            topic: messageObject.topic,
            rawJson: messageObject.rawJson,
            realTime: messageObject.realTime,
            sincePrev: messageObject.sincePrev,
            sinceSame: messageObject.sinceSame,
            espTimestamp: messageObject.espTimestamp,
            browserTimestamp: messageObject.browserTimestamp,
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
                    rawJson: msg.rawJson,
                    realTime: msg.realTime,
                    sincePrev: msg.sincePrev,
                    sinceSame: msg.sinceSame,
                    espTimestamp: msg.espTimestamp || Date.now(),
                    isSeparator: msg.isSeparator || false,
                    message: msg.message
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

        tableBody.innerHTML = this.messageSequence.map(item => this.createRowHtml(item)).join('');
    }

    formatRawJson(rawJson) {
        try {
            const parsed = JSON.parse(rawJson);
            const formatted = JSON.stringify(parsed, null, 2);
            
            // ONLY check if object has 'error' property
            const hasErrorProperty = parsed.error !== undefined;
            
            if (hasErrorProperty) {
                return `<div class="raw-json error-highlight">${this.syntaxHighlight(formatted)}</div>`;
            }
            
            return `<div class="raw-json">${this.syntaxHighlight(formatted)}</div>`;
        } catch (error) {
            // If JSON parsing fails but contains "error" key text
            if (rawJson.toLowerCase().includes('"error"')) {
                return `<div class="raw-json error-highlight">${rawJson}</div>`;
            }
            return `<div class="raw-json">${rawJson}</div>`;
        }
    }

    syntaxHighlight(json) {
        return json.replace(/("(\\u[a-zA-Z0-9]{4}|\\[^u]|[^\\"])*"(\s*:)?|\b(true|false|null)\b|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?)/g, match => {
            let cls = 'raw-json-number';
            
            // ONLY check for "error" keys (not values/messages)
            const isErrorKey = /^"error"(\s*:)?$/i.test(match);
            
            if (isErrorKey) {
                cls = 'raw-json-error';
            } else if (/^"/.test(match)) {
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

    // ========== CLEANUP & STATUS ==========

    async clearTable() {
        const tableBody = FormHelper.getElement('tableBody');
        
        if (tableBody) {
            tableBody.innerHTML = '';
            localStorage.removeItem('mqttDebugMessages');
            this.messageSequence = [];
            this.updateStatusMessages();
            
            try {
                await ApiClient.post('/cleartable', {});
                StatusManager.showStatus('Table cleared and timing reset', 'success');
                console.log('✅ Table cleared and timing reset');
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