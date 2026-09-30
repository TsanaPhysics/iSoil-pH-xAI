<!DOCTYPE html>
<html lang="th">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RBRU AIoT | Smart Soil pH Telemetry Dashboard</title>
    
    <!-- Google Fonts -->
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;600;700&family=Plus+Jakarta+Sans:wght@400;500;600;700;800&display=swap" rel="stylesheet">
    
    <!-- Chart.js via CDN -->
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"></script>
    
    <!-- Custom Stylesheet -->
    <link rel="stylesheet" href="css/dashboard.css">
</head>
<body>

<div class="dashboard-container">
    
    <!-- Header Bar -->
    <header class="header-bar">
        <div class="brand-wrapper">
            <div class="brand-logo-badge">🌱</div>
            <div class="brand-info">
                <h1>iSoil pH xAI MONITOR</h1>
                <p>Digital Agriphysics & AI Error Compensation | Wio Terminal Edge TinyML</p>
            </div>
        </div>
        
        <div class="header-status-group">
            <div id="systemStatusBadge" class="pulse-badge">
                <span class="pulse-dot"></span>
                <span>CONNECTING...</span>
            </div>
            
            <button id="btnTogglePoll" class="btn-action">⏸ Pause Stream</button>
            <a href="api/export_csv.php" class="btn-action btn-primary" title="ดาวน์โหลดชุดข้อมูล CSV ทั้งหมด">
                📥 Export CSV
            </a>
            <button id="btnSimulate" class="btn-action" title="จำลองข้อมูลเพื่อทดสอบหน้าจอ">
                🧪 Test Sample
            </button>
            <a href="docs/manual.pdf" target="_blank" class="btn-action" title="เปิดคู่มือวิชาการ PDF">
                📄 Manual PDF
            </a>
        </div>
    </header>

    <!-- KPI Metric Cards Grid -->
    <section class="kpi-grid">
        
        <!-- Card 1: AI Predicted pH -->
        <div class="kpi-card card-ai-ph">
            <div class="kpi-header">
                <span class="kpi-title">AI-Compensated pH (ANN)</span>
                <span class="kpi-icon">🧠</span>
            </div>
            <div class="kpi-value-row">
                <span id="kpiAIPH" class="kpi-huge-number">--.--</span>
                <span id="kpiDeltaTag" class="kpi-delta-tag">+0.000 pH</span>
            </div>
            <div class="kpi-subtext">Real-time Multilayer Perceptron TinyML Model</div>
        </div>

        <!-- Card 2: Traditional Nernst pH -->
        <div class="kpi-card card-trad-ph">
            <div class="kpi-header">
                <span class="kpi-title">Traditional (Nernst Linear)</span>
                <span class="kpi-icon">📐</span>
            </div>
            <div class="kpi-value-row">
                <span id="kpiTradPH" class="kpi-huge-number" style="color: var(--color-orange); text-shadow:none;">--.--</span>
            </div>
            <div class="kpi-subtext">First-Principles Electrochemical Physical Model</div>
        </div>

        <!-- Card 3: Cell Potential & Temperature -->
        <div class="kpi-card card-sensor">
            <div class="kpi-header">
                <span class="kpi-title">Cell Potential & Temp</span>
                <span class="kpi-icon">⚡</span>
            </div>
            <div class="kpi-value-row">
                <span id="kpiVoltage" class="kpi-huge-number" style="font-size: 2.2rem; color: var(--color-cyan); text-shadow:none;">-.--- V</span>
            </div>
            <div class="kpi-subtext">
                Temperature: <strong id="kpiTemp" style="color:#fff;">--.- °C</strong> | 12-bit ADC (SAMD51)
            </div>
        </div>

        <!-- Card 4: Research Dataset & Target Buffer -->
        <div class="kpi-card card-stats">
            <div class="kpi-header">
                <span class="kpi-title">Research Dataset Stats</span>
                <span class="kpi-icon">📊</span>
            </div>
            <div class="kpi-value-row">
                <span id="kpiTotalRows" class="kpi-huge-number" style="font-size: 2.2rem; color: var(--color-gold); text-shadow:none;">0</span>
                <span class="kpi-subtext" style="font-size:1rem; font-weight:700;">Records</span>
            </div>
            <div class="kpi-subtext">
                Target: <strong id="kpiTargetBuf" style="color:#fff;">FIELD</strong> | Optimal Soil: <strong id="kpiOptimalPercent" style="color:var(--color-green);">--%</strong>
            </div>
        </div>

    </section>

    <!-- Soil Diagnostic Banner -->
    <section class="soil-diagnostic-card">
        <div style="display:flex; align-items:center; gap: 16px;">
            <div id="soilStatusBadge" class="soil-status-pill status-neutral">
                ● ANALYZING SOIL...
            </div>
            <div id="soilAdvisoryText" class="soil-advisory-text">
                ระบบกำลังประมวลผลข้อมูลทางเคมีฟิสิกส์จากโพรบวัด เพื่อประเมินความเหมาะสมในการปลูกทุเรียนและพืชสวน...
            </div>
        </div>
        <div style="display:flex; gap:8px;">
            <a href="docs/AI_SOIL_PH_METER_MANUAL.md" target="_blank" class="btn-action" style="font-size:0.78rem;">
                คู่มือการแปลผล
            </a>
        </div>
    </section>

    <!-- Content Grid: Chart & Real-time Table -->
    <section class="content-grid">
        
        <!-- Panel 1: Real-time Comparison Chart -->
        <div class="panel-card">
            <div class="panel-header">
                <div class="panel-title">
                    <span>📈 Dual-Model Real-time Comparison Chart</span>
                </div>
                <div style="font-size:0.8rem; color:var(--text-muted);">
                    Sampling Rate: 2.0s
                </div>
            </div>
            <div class="chart-container">
                <canvas id="phTrendChart"></canvas>
            </div>
        </div>

        <!-- Panel 2: Live Data Stream Table -->
        <div class="panel-card">
            <div class="panel-header">
                <div class="panel-title">
                    <span>📋 Live Stream Logs</span>
                </div>
                <div style="font-size:0.78rem; color:var(--text-muted);">
                    Latest 10 Records
                </div>
            </div>
            <div class="table-wrapper">
                <table class="data-table">
                    <thead>
                        <tr>
                            <th>ID</th>
                            <th>Time</th>
                            <th>pH (AI)</th>
                            <th>pH (Trad)</th>
                            <th>Volt</th>
                            <th>Temp</th>
                            <th>Buffer</th>
                        </tr>
                    </thead>
                    <tbody id="tableBody">
                        <tr>
                            <td colspan="7" style="text-align:center; color:var(--text-dim); padding:20px;">
                                กำลังรอสัญญาณข้อมูลจาก Wio Terminal...
                            </td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </div>

    </section>

    <!-- Footer Attribution -->
    <footer class="footer-bar">
        <p><strong>โครงการวิจัย:</strong> การพัฒนาต้นแบบเครื่องวัดความเป็นกรด-ด่างของดินแบบพกพาภาคสนามความแม่นยำสูงด้วยการชดเชยความคลาดเคลื่อนโดยปัญญาประดิษฐ์</p>
        <p>ทุนวิจัยสำหรับบุคลากรสายวิชาการ กองทุนวิจัย มหาวิทยาลัยราชภัฏรำไพพรรณี ประจำปีงบประมาณ พ.ศ. 2569</p>
        <p style="margin-top: 6px; color: var(--text-muted);">
            คณะผู้วิจัย: อาจารย์ธนพัฒน์ ถิระวุฒิ | ผู้ช่วยศาสตราจารย์ ดร.ชีวะ ทัศนา | ผู้ช่วยศาสตราจารย์ ดร.นันทพร มูลรังษี
        </p>
    </footer>

</div>

<!-- Custom Dashboard Logic -->
<script src="js/dashboard.js"></script>

</body>
</html>
