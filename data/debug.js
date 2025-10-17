class DebugConsole {
    constructor() {
        this.isEnabled = false;
        this.init();
    }

    init() {
        this.loadDebugState();
        this.bindEvents();
        this.startMessagePolling(); // ADD THIS!
    }

    bindEvents() {
        document.getElementById('debugEnabled').addEventListener('change', (e) => {
            this.toggleDebugMode(e.target.checked);
        });

        document.getElementById('clearConsole').addEventListener('click', () => {
            this.clearConsole();
        });
    }

    async loadDebugState() {
        try {
            const response = await fetch('/getdebugstate');
            const data = await response.json();
            this.isEnabled = data.enabled;
            this.updateToggle();
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
                this.addMessage(`Debug ${enabled ? 'ENABLED - MQTT + Console' : 'DISABLED - MQTT only'}`);
            }
        } catch (error) {
            this.addMessage('Error toggling debug mode');
        }
    }

    updateToggle() {
        const checkbox = document.getElementById('debugEnabled');
        const status = document.getElementById('debugStatus');
        
        checkbox.checked = this.isEnabled;
        status.textContent = this.isEnabled ? 'ON' : 'OFF';
        status.style.color = this.isEnabled ? '#10b981' : '#ef4444';
        
        // if (!this.isEnabled) {
        //     this.clearConsole();
        //     this.addMessage('Debug mode is OFF. Enable to see MQTT messages here.');
        // }
    }

    addMessage(message) {
        const consoleElement = document.getElementById('debugConsole');
        const messageElement = document.createElement('div');
        messageElement.className = 'console-line';
        messageElement.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
        consoleElement.appendChild(messageElement);
        consoleElement.scrollTop = consoleElement.scrollHeight;
    }

    clearConsole() {
        document.getElementById('debugConsole').innerHTML = '';
    }

    async checkForMessages() {
        if (!this.isEnabled) return;

        try {
            const response = await fetch('/getdebugmessages');
            const messages = await response.json();
            
            messages.forEach(msg => {
                this.addMessage(`MQTT [${msg.topic}]: ${msg.message}`);
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