<?php
/**
 * PicoKey API — api.php
 * Place in /public_html/pico/ on shared hosting
 * Requires: PHP 7.4+, write permission on ./data/
 */

define('TOKEN',    'CHANGE_THIS_SECRET_TOKEN_32CHARS');  // must match ESP8266
define('DATA_DIR', __DIR__ . '/data/');
define('QUEUE_F',  DATA_DIR . 'queue.json');
define('STATUS_F', DATA_DIR . 'status.json');
define('COMBOS_F', DATA_DIR . 'combos.json');
define('AUTH_F',   DATA_DIR . 'auth.json');   // dashboard password hash
define('MAX_QUEUE',50);

if (!is_dir(DATA_DIR)) mkdir(DATA_DIR, 0750, true);
foreach ([QUEUE_F, COMBOS_F] as $f)
    if (!file_exists($f)) file_put_contents($f, '[]');
if (!file_exists(STATUS_F)) file_put_contents(STATUS_F, '{}'); // FIX: must be object not array
if (!file_exists(AUTH_F))
    file_put_contents(AUTH_F, json_encode(['hash' => password_hash('picokey', PASSWORD_BCRYPT)]));

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, X-Auth-Token');
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') { http_response_code(204); exit; }

$input  = json_decode(file_get_contents('php://input'), true) ?? [];
$action = $_GET['action'] ?? $input['action'] ?? '';

// ── Route ─────────────────────────────────────────────────────────────────
switch ($action) {

    // ── DEVICE: poll for next command ────────────────────────────────────
    case 'poll':
        deviceAuth();
        $q = readJson(QUEUE_F);
        if (empty($q)) { echo json_encode(['status'=>'empty']); break; }
        $cmd = array_shift($q);
        writeJson(QUEUE_F, $q);
        echo json_encode(['status'=>'ok','cmd'=>$cmd]);
        break;

    // ── DEVICE: heartbeat ────────────────────────────────────────────────
    case 'heartbeat':
        deviceAuth();
        $s = ['online'=>true,'ip'=>$input['ip']??'','ts'=>time(),'state'=>$input['state']??'alive'];
        writeJson(STATUS_F, $s);
        echo json_encode(['status'=>'ok']);
        break;

    // ── DEVICE: ACK from Pico ────────────────────────────────────────────
    case 'ack':
        deviceAuth();
        echo json_encode(['status'=>'ok']);
        break;

    // ── DASHBOARD: login ─────────────────────────────────────────────────
    case 'login':
        $pw   = $input['password'] ?? '';
        $auth = readJson(AUTH_F);
        if (password_verify($pw, $auth['hash'])) {
            $token = bin2hex(random_bytes(16));
            file_put_contents(DATA_DIR.'session.txt', $token.':'.time());
            echo json_encode(['status'=>'ok','session'=>$token]);
        } else {
            http_response_code(401);
            echo json_encode(['status'=>'error','msg'=>'Invalid password']);
        }
        break;

    // ── DASHBOARD: enqueue command ────────────────────────────────────────
    case 'send':
        dashAuth();
        $cmd = $input['cmd'] ?? null;
        if (!$cmd || !isset($cmd['t'])) { badRequest('Missing cmd'); break; }
        $q = readJson(QUEUE_F);
        if (count($q) >= MAX_QUEUE) array_shift($q);
        $q[] = json_encode($cmd);
        writeJson(QUEUE_F, $q);
        echo json_encode(['status'=>'ok','queued'=>count($q)]);
        break;

    // ── DASHBOARD: get device status ──────────────────────────────────────
    case 'status':
        dashAuth();
        $s = readJson(STATUS_F);
        $online = isset($s['ts']) && (time()-$s['ts'] < 60);
        $s['online'] = $online;
        echo json_encode(['status'=>'ok','device'=>$s]);
        break;

    // ── DASHBOARD: save custom combos ─────────────────────────────────────
    case 'save_combos':
        dashAuth();
        $combos = $input['combos'] ?? [];
        writeJson(COMBOS_F, $combos);
        echo json_encode(['status'=>'ok']);
        break;

    // ── DASHBOARD: get custom combos ──────────────────────────────────────
    case 'get_combos':
        dashAuth();
        echo json_encode(['status'=>'ok','combos'=>readJson(COMBOS_F)]);
        break;

    // ── DASHBOARD: clear queue ────────────────────────────────────────────
    case 'clear_queue':
        dashAuth();
        writeJson(QUEUE_F, []);
        echo json_encode(['status'=>'ok']);
        break;

    default:
        http_response_code(400);
        echo json_encode(['status'=>'error','msg'=>'Unknown action']);
}

// ── Helpers ────────────────────────────────────────────────────────────────
function readJson($f) {
    $c = @file_get_contents($f);
    return $c ? (json_decode($c, true) ?? []) : [];
}
function writeJson($f, $d) {
    file_put_contents($f, json_encode($d), LOCK_EX);
}
function deviceAuth() {
    global $input;
    $t = $_GET['token'] ?? $input['token'] ?? '';
    if ($t !== TOKEN) { http_response_code(403); echo json_encode(['status'=>'forbidden']); exit; }
}
function dashAuth() {
    $h = $_SERVER['HTTP_X_AUTH_TOKEN'] ?? ($_GET['session'] ?? '');
    $stored = @file_get_contents(DATA_DIR.'session.txt');
    if (!$stored) { http_response_code(401); echo json_encode(['status'=>'auth_required']); exit; }
    $parts = explode(':', trim($stored), 2);
    $tok = $parts[0] ?? ''; $ts = $parts[1] ?? '0';
    if ($h !== $tok || (time()-intval($ts)) > 86400*7) {
        http_response_code(401); echo json_encode(['status'=>'auth_required']); exit;
    }
}
function badRequest($msg) {
    http_response_code(400); echo json_encode(['status'=>'error','msg'=>$msg]); exit;
}
