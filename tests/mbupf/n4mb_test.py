"""PFCP-level tests of the MB-UPF N4mb behaviour (TS 29.244 V18.10.0).

Starts open5gs-upfd on 127.0.0.31 inside the private network namespace created by run.sh, sends PFCP messages and
checks the answers. Standard library only. Each test is a function named t_<case>; a test passes if it returns
without raising AssertionError.
"""
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time

UPFD = sys.argv[1]
FILTER = sys.argv[2:] 
UPF = ('127.0.0.31', 8805)
CP = ('127.0.0.77', 8805)

# IE types
T_CREATE_PDR, T_PDI, T_CREATE_FAR, T_CREATE_QER = 1, 2, 3, 7
T_UPDATE_FAR = 10
T_ADD_UNICAST, T_REMOVE_UNICAST, T_UNICAST_ID, T_TLM = 302, 304, 309, 30
MODIFICATION_REQUEST = 52
T_CAUSE, T_SRC_IF, T_NET_INST, T_GATE, T_PRECEDENCE, T_OFFENDING, T_DST_IF, T_APPLY = 19, 20, 22, 25, 29, 40, 42, 44
T_PDR_ID, T_F_SEID, T_NODE_ID, T_OHC, T_FAR_ID, T_QER_ID, T_QFI = 56, 57, 60, 84, 108, 109, 124
T_CREATE_TE, T_CREATED_TE, T_TE_ID = 127, 128, 131
T_IPMC_INFO, T_IPMC_ADDR, T_SRC_IP = 188, 191, 192
T_PARTIAL = 272
T_N4MB_CTRL, T_MBS_MC_PARAMS, T_N4MB_INFO, T_MBS_ID, T_MC_TRANSPORT, T_N4MB_FLAGS, T_LIT = 300, 301, 303, 305, 306, 307, 308

CAUSE_ACCEPTED, CAUSE_PARTIAL, CAUSE_REJECTED, CAUSE_CONTEXT_NOT_FOUND = 1, 3, 64, 65
CAUSE_MANDATORY_MISSING, CAUSE_CONDITIONAL_MISSING, CAUSE_INVALID_LENGTH, CAUSE_MANDATORY_INCORRECT = 66, 67, 68, 69
CAUSE_RULE_FAILURE, CAUSE_NO_RESOURCES, CAUSE_SERVICE_NOT_SUPPORTED, CAUSE_SYSTEM_FAILURE = 73, 75, 76, 77
CAUSE_RESTORATION_FAILURE = 86



def ie(t, v):
    if isinstance(v, list):
        v = b''.join(v)
    return struct.pack('>HH', t, len(v)) + v


def parse_ies(body):
    out = []
    pos = 0
    while pos + 4 <= len(body):
        t, l = struct.unpack('>HH', body[pos:pos + 4])
        out.append((t, body[pos + 4:pos + 4 + l]))
        pos += 4 + l
    return out


def msg(mtype, seq, body, seid=None):
    if seid is None:
        return bytes([0x20, mtype]) + struct.pack('>H', 4 + len(body)) + seq.to_bytes(3, 'big') + b'\x00' + body
    return (bytes([0x21, mtype]) + struct.pack('>H', 12 + len(body)) + struct.pack('>Q', seid) +
            seq.to_bytes(3, 'big') + b'\x00' + body)


class Upf:
    def __init__(self):
        self.dir = tempfile.mkdtemp(prefix='mbupf-test-')
        self.cfg = os.path.join(self.dir, 'upf.yaml')
        self.log = os.path.join(self.dir, 'upf.log')
        self.write_config('0')
        self.proc = None
        self.sock = None
        self.seq = 100
        self.pre_net()
        self.start()

    def write_config(self, port_yaml, extra_upf='', gtpu_port=None):
        open(self.cfg, 'w').write("""logger:
  level: debug
  file:
    path: %s
global:
upf:
  pfcp:
    server:
      - address: 127.0.0.31
  gtpu:
    server:
      - address: 127.0.0.31
%s  session:
    - subnet: 10.46.0.0/16
      gateway: 10.46.0.1
      dev: ogstunt
  mbs:
    udptunnel:
      address: 127.0.0.31
      port: %s
%s""" % (self.log, '        port: %d\n' % gtpu_port if gtpu_port else '', port_yaml, extra_upf))

    def restart(self, port_yaml='0', extra_upf='', gtpu_port=None):
        self.stop()
        self.write_config(port_yaml, extra_upf, gtpu_port)
        self.start()

    def start(self):
        self.err = os.path.join(self.dir, 'upf.err')
        self.proc = subprocess.Popen([UPFD, '-c', self.cfg], stdout=subprocess.DEVNULL, stderr=open(self.err, 'w'))
        time.sleep(1.5)
        assert self.proc.poll() is None, 'upfd did not start, see ' + self.log
        if self.sock:
            self.sock.close()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(CP)
        self.sock.settimeout(2.0)
        self.associate()
        self.setup_net()

    def pre_net(self):
        """Interfaces the UPF needs when it starts: the TUN device it attaches to, and a veth pair whose one end
        is the multicast router's input interface (the test sends from the other end)."""
        for cmd in (['ip', 'tuntap', 'add', 'dev', 'ogstunt', 'mode', 'tun'],
                    ['ip', 'link', 'add', 'v0', 'type', 'veth', 'peer', 'name', 'v1'],
                    ['ip', 'addr', 'add', '10.99.0.1/24', 'dev', 'v0'],
                    ['ip', 'addr', 'add', '10.99.0.2/24', 'dev', 'v1'],
                    ['ip', 'link', 'set', 'v0', 'up'],
                    ['ip', 'link', 'set', 'v1', 'up'],
                    # v1's address is a local one: let v0 accept it as the source of what arrives from v1
                    ['sysctl', '-q', '-w', 'net.ipv4.conf.all.rp_filter=0', 'net.ipv4.conf.v0.rp_filter=0',
                     'net.ipv4.conf.v0.accept_local=1']):
            subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def setup_net(self):
        """Routes for the data path: multicast sent to the UPF's TUN device, N3mb multicast looped on lo."""
        for cmd in (['ip', 'addr', 'add', '10.46.0.1/16', 'dev', 'ogstunt'],
                    ['ip', 'addr', 'add', '10.46.0.2/16', 'dev', 'ogstunt'],
                    ['ip', 'link', 'set', 'ogstunt', 'up'],
                    ['ip', 'route', 'add', '232.0.0.0/8', 'dev', 'ogstunt'],
                    ['ip', 'route', 'add', '239.0.0.0/8', 'dev', 'lo'],
                    ['ip', 'link', 'set', 'lo', 'multicast', 'on']):
            subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def log_text(self):
        return open(self.log, errors='replace').read()

    def alive(self):
        return self.proc.poll() is None

    def stop(self):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(5)
            except subprocess.TimeoutExpired:
                self.proc.kill()

    def nextseq(self):
        self.seq += 1
        return self.seq

    def xchg(self, data):
        self.sock.sendto(data, UPF)
        out = []
        try:
            while True:
                d, addr = self.sock.recvfrom(65535)
                if d[1] == 1:  # heartbeat request: answer, keep the association
                    self.sock.sendto(msg(2, int.from_bytes(d[4:7], 'big'), ie(96, struct.pack('>I', 3900000000))), addr)
                    continue
                out.append(d)
                break  # one response per request
        except socket.timeout:
            pass
        return out

    def associate(self):
        body = ie(T_NODE_ID, b'\x00' + socket.inet_aton('127.0.0.77')) + ie(96, struct.pack('>I', 3900000000))
        r = self.xchg(msg(5, self.nextseq(), body))
        assert r, 'no association response'

    def request(self, mtype, ies, seid=0):
        body = b''.join(ies)
        r = self.xchg(msg(mtype, self.nextseq(), body, seid=seid))
        if not r:
            return None
        d = r[0]
        h = 4
        seid_r = None
        if d[0] & 1:
            seid_r = struct.unpack('>Q', d[4:12])[0]
            h = 12
        return dict(type=d[1], seid=seid_r, ies=parse_ies(d[h + 4:]))

    def establish(self, ies):
        return self.request(50, ies, seid=0)

    def delete(self, upf_seid):
        return self.request(54, [], seid=upf_seid)


# ---- request building -------------------------------------------------------------------------------------------

def fseid(cp_seid=0x1000):
    return ie(T_F_SEID, b'\x02' + struct.pack('>Q', cp_seid) + socket.inet_aton('127.0.0.77'))


def node_id():
    return ie(T_NODE_ID, b'\x00' + socket.inet_aton('127.0.0.77'))


def tmgi_id(service=b'\x00\x00\x01', plmn=b'\x00\xf1\x10'):
    return ie(T_MBS_ID, b'\x01' + service + plmn)


def pdr(pdr_id=1, precedence=100, far_id=1, qer_id=1, pdi_extra=(), pdi=None, drop=()):
    if pdi is None:
        pdi = ie(T_PDI, [ie(T_SRC_IF, b'\x01')] + list(pdi_extra))
    parts = [ie(T_PDR_ID, struct.pack('>H', pdr_id)), ie(T_PRECEDENCE, struct.pack('>I', precedence)), pdi,
             ie(T_FAR_ID, struct.pack('>I', far_id)), ie(T_QER_ID, struct.pack('>I', qer_id))]
    parts = [p for p in parts if struct.unpack('>H', p[:2])[0] not in drop]
    return ie(T_CREATE_PDR, parts)


def far(far_id=1, apply=0x0008, params=True, drop=(), mc_extra=()):
    parts = [ie(T_FAR_ID, struct.pack('>I', far_id)), ie(T_APPLY, struct.pack('>H', apply))]
    if params:
        parts.append(ie(T_MBS_MC_PARAMS, [ie(T_DST_IF, b'\x00'), ie(T_OHC, b'\x00\x04')] + list(mc_extra)))
    parts = [p for p in parts if struct.unpack('>H', p[:2])[0] not in drop]
    return ie(T_CREATE_FAR, parts)


def qer(qer_id=1, gate=0, qfi=1, indications=None, drop=()):
    parts = [ie(T_QER_ID, struct.pack('>I', qer_id)), ie(T_GATE, bytes([gate])), ie(T_QFI, bytes([qfi]))]
    parts = [p for p in parts if struct.unpack('>H', p[:2])[0] not in drop]
    if indications is not None:
        parts.append(ie(319, bytes([indications])))
    return ie(T_CREATE_QER, parts)


def lit_choose(v4=True, v6=False):
    return ie(T_LIT, bytes([(1 if v4 else 0) | (2 if v6 else 0) | 4]))


def n4mb_ctrl(mbs_id=None, flags=0x01, extra=()):
    parts = [mbs_id if mbs_id is not None else tmgi_id()]
    if flags is not None:
        parts.append(ie(T_N4MB_FLAGS, bytes([flags])))
    parts += list(extra)
    return ie(T_N4MB_CTRL, parts)


def good(cp_seid=0x1000, flags=0x01):
    """Tunnel ingress (Create Traffic Endpoint with CHOOSE), FSSM forwarding, low layer SSM requested."""
    return [node_id(), fseid(cp_seid), pdr(), far(), qer(), ie(T_CREATE_TE, [lit_choose()]), n4mb_ctrl(flags=flags)]


def cause(r):
    for t, v in r['ies']:
        if t == T_CAUSE:
            return v[0]
    return None


def offending(r):
    for t, v in r['ies']:
        if t == T_OFFENDING:
            return struct.unpack('>H', v)[0]
    return None


def upf_seid(r):
    for t, v in r['ies']:
        if t == T_F_SEID:
            return struct.unpack('>Q', v[1:9])[0]
    return None


def find(r, t):
    for tt, v in r['ies']:
        if tt == t:
            return v
    return None


def replace_ie(ies, t, new):
    """Returns the list with every top-level IE of type t replaced by new (None removes it)."""
    out = []
    for i in ies:
        if struct.unpack('>H', i[:2])[0] == t:
            if new is not None:
                out.append(new)
        else:
            out.append(i)
    return out


U = None


def accepted(r, what):
    assert r is not None, what + ': no response'
    assert cause(r) == CAUSE_ACCEPTED, '%s: cause %s offending %s' % (what, cause(r), offending(r))
    return upf_seid(r)


def cleanup(r):
    s = upf_seid(r) if r else None
    if s:
        U.delete(s)


def accepted_and_deleted(r, what):
    s = accepted(r, what)
    d = U.delete(s)
    assert d and cause(d) == CAUSE_ACCEPTED, what + ': delete'


TESTS = []


def test(f):
    TESTS.append(f)
    return f


@test
def t_baseline():
    r = U.establish(good())
    s = accepted(r, 'baseline')
    assert s, 'no UP F-SEID'
    assert find(r, T_N4MB_INFO) is not None, 'no MBS Session N4mb Information'
    assert find(r, T_CREATED_TE) is not None, 'no Created Traffic Endpoint'
    d = U.delete(s)
    assert cause(d) == CAUSE_ACCEPTED


@test
def t_U01_1_missing_cp_fseid():
    r = U.establish(replace_ie(good(), T_F_SEID, None))
    assert r and cause(r) == CAUSE_MANDATORY_MISSING, 'cause %s' % (cause(r) if r else None)
    assert offending(r) == T_F_SEID, 'offending IE %s, want %d' % (offending(r), T_F_SEID)


@test
def t_U01_3_missing_node_id():
    r = U.establish(replace_ie(good(), T_NODE_ID, None))
    assert r and cause(r) == CAUSE_MANDATORY_MISSING, 'cause %s' % (cause(r) if r else None)
    assert offending(r) == T_NODE_ID, 'offending IE %s, want %d' % (offending(r), T_NODE_ID)
    # the rejected request leaves nothing behind: the same CP F-SEID can be used again
    accepted_and_deleted(U.establish(good()), 'after the rejection')


@test
def t_U01_1_pool_exhausted_is_not_a_missing_ie():
    # max.mbs.mbs_sessions defaults to 20: the 21st session is refused for lack of resources, not for a missing IE
    seids = []
    try:
        for i in range(20):
            seids.append(accepted(U.establish(good(cp_seid=0x2000 + i)), 'session %d' % i))
        r = U.establish(good(cp_seid=0x2100))
        assert r and cause(r) == CAUSE_NO_RESOURCES, 'cause %s' % (cause(r) if r else None)
    finally:
        for sid in seids:
            U.delete(sid)
    accepted_and_deleted(U.establish(good(cp_seid=0x2200)), 'after the sessions were deleted')


@test
def t_U01_4_no_create_pdr():
    r = U.establish(replace_ie(good(), T_CREATE_PDR, None))
    assert r and cause(r) == CAUSE_MANDATORY_MISSING, 'cause %s' % (cause(r) if r else None)
    assert offending(r) == T_CREATE_PDR, 'offending IE %s' % offending(r)
    accepted_and_deleted(U.establish(good()), 'after the rejection')


@test
def t_U01_5_no_create_far():
    r = U.establish(replace_ie(good(), T_CREATE_FAR, None))
    assert r and cause(r) == CAUSE_MANDATORY_MISSING, 'cause %s' % (cause(r) if r else None)
    assert offending(r) == T_CREATE_FAR, 'offending IE %s' % offending(r)
    accepted_and_deleted(U.establish(good()), 'after the rejection')


def ssm_id(dst_flags, dst, src_flags, src, first=0x02, tail=b''):
    return ie(T_MBS_ID, bytes([first, dst_flags]) + dst + bytes([src_flags]) + src + tail)


V4G, V4S = socket.inet_aton('232.1.1.1'), socket.inet_aton('10.1.1.1')


@test
def t_U02_5_ssm_address_type_and_length():
    # type 0 with length 4 and type 1 with length 16 are the only valid pairs (TS 29.244 cl. 8.2.207)
    s = accepted(U.establish(good(cp_seid=0x3001)[:-1] + [n4mb_ctrl(ssm_id(0x04, V4G, 0x04, V4S))]), 'IPv4 SSM')
    U.delete(s)
    v6 = bytes(range(1, 17))
    s = accepted(U.establish(good(cp_seid=0x3002)[:-1] + [n4mb_ctrl(ssm_id(0x50, v6, 0x50, v6))]), 'IPv6 SSM')
    U.delete(s)
    bad = {
        'type 0 length 16': ssm_id(0x10, v6, 0x04, V4S),
        'type 1 length 4': ssm_id(0x44, V4G, 0x04, V4S),
        'length 0': ssm_id(0x00, b'', 0x04, V4S),
        'length 63': ssm_id(0x3f, bytes(63), 0x04, V4S),
        'source type 2': ssm_id(0x04, V4G, 0x84, V4S),
        'truncated source': ie(T_MBS_ID, bytes([0x02, 0x04]) + V4G + bytes([0x04]) + V4S[:2]),
        'trailing octet': ssm_id(0x04, V4G, 0x04, V4S, tail=b'\x00'),
        'empty': ie(T_MBS_ID, b''),
    }
    for name, ident in bad.items():
        r = U.establish(good(cp_seid=0x3010)[:-1] + [n4mb_ctrl(ident)])
        assert r is not None, name + ': no answer (upfd alive: %s)' % U.alive()
        assert cause(r) == CAUSE_MANDATORY_INCORRECT, '%s: cause %s' % (name, cause(r))
        assert offending(r) == T_MBS_ID, '%s: offending IE %s' % (name, offending(r))
    accepted_and_deleted(U.establish(good(cp_seid=0x3020)), 'after the rejections')


@test
def t_U02_6_nid_is_stored():
    ident = ie(T_MBS_ID, b'\x05' + b'\x00\x00\x01\x00\xf1\x10' + bytes.fromhex('123456789ab0'))
    s = accepted(U.establish(good(cp_seid=0x3030)[:-1] + [n4mb_ctrl(ident)]), 'TMGI with NID')
    assert 'NID [123456789ab]' in U.log_text(), 'NID not stored'
    U.delete(s)
    ident = ie(T_MBS_ID, b'\x05' + b'\x00\x00\x01\x00\xf1\x10' + bytes.fromhex('123456'))
    r = U.establish(good(cp_seid=0x3031)[:-1] + [n4mb_ctrl(ident)])
    assert r and cause(r) == CAUSE_MANDATORY_INCORRECT, 'short NID: cause %s' % (cause(r) if r else None)


@test
def t_U03_2_pdr_without_precedence():
    r = U.establish(replace_ie(good(cp_seid=0x4001), T_CREATE_PDR, pdr(drop=(T_PRECEDENCE,))))
    assert r and cause(r) == CAUSE_MANDATORY_MISSING, 'cause %s' % (cause(r) if r else None)
    assert offending(r) == T_PRECEDENCE, 'offending IE %s' % offending(r)
    accepted_and_deleted(U.establish(good(cp_seid=0x4002)), 'after the rejection')


def with_te(ies, *te_ies):
    return replace_ie(ies, T_CREATE_TE, ie(T_CREATE_TE, list(te_ies)))


def created_te(r):
    v = find(r, T_CREATED_TE)
    return dict(parse_ies(v)) if v is not None else None


@test
def t_U10_4_V4_and_V6_together():
    r = U.establish(with_te(good(cp_seid=0x5001), ie(T_LIT, b'\x07')))
    assert r and cause(r) == CAUSE_MANDATORY_INCORRECT, 'cause %s' % (cause(r) if r else None)
    assert offending(r) == T_LIT, 'offending IE %s' % offending(r)
    r = U.establish(with_te(good(cp_seid=0x5002), ie(T_LIT, b'\x03' + struct.pack('>H', 4000) + bytes(4) + bytes(16))))
    assert r and cause(r) == CAUSE_MANDATORY_INCORRECT, 'CH clear: cause %s' % (cause(r) if r else None)
    accepted_and_deleted(U.establish(good(cp_seid=0x5003)), 'after the rejections')


@test
def t_U10_5_neither_V4_nor_V6():
    for name, val in (('CH only', b'\x04'), ('nothing', b'\x00')):
        r = U.establish(with_te(good(cp_seid=0x5011), ie(T_LIT, val)))
        assert r and cause(r) == CAUSE_MANDATORY_INCORRECT, '%s: cause %s' % (name, cause(r) if r else None)
        assert offending(r) == T_LIT, '%s: offending IE %s' % (name, offending(r))
    r = U.establish(with_te(good(cp_seid=0x5012), ie(T_LIT, b'\x05\x00\x00')))
    assert r and cause(r) == CAUSE_INVALID_LENGTH, 'CH with an address field: cause %s' % (cause(r) if r else None)
    r = U.establish(with_te(good(cp_seid=0x5013), ie(T_LIT, b'\x01\x0f\xa0\x7f')))
    assert r and cause(r) == CAUSE_INVALID_LENGTH, 'short address: cause %s' % (cause(r) if r else None)
    r = U.establish(with_te(good(cp_seid=0x5014), ie(T_LIT, b'')))
    assert r and cause(r) == CAUSE_INVALID_LENGTH, 'empty: cause %s' % (cause(r) if r else None)
    accepted_and_deleted(U.establish(good(cp_seid=0x5015)), 'after the rejections')


@test
def t_U10_6_cp_gives_address_and_port():
    lit = ie(T_LIT, b'\x01' + struct.pack('>H', 39123) + socket.inet_aton('127.0.0.31'))
    r = U.establish(with_te(good(cp_seid=0x5021), ie(T_TE_ID, b'\x01'), lit))
    s = accepted(r, 'CH=0 IPv4')
    te = created_te(r)
    assert te and T_LIT in te, 'no Created Traffic Endpoint with a Local Ingress Tunnel'
    assert te[T_LIT] == b'\x01' + struct.pack('>H', 39123) + socket.inet_aton('127.0.0.31'), te[T_LIT].hex()
    # the second session asking for the same address and port cannot be given it
    r2 = U.establish(with_te(good(cp_seid=0x5022), ie(T_TE_ID, b'\x01'), lit))
    assert r2 and cause(r2) != CAUSE_ACCEPTED, 'the same address and port was given twice'
    assert offending(r2) == T_LIT, 'offending IE %s' % offending(r2)
    d = U.delete(s)
    assert cause(d) == CAUSE_ACCEPTED
    # released with the session: it can be asked for again
    accepted_and_deleted(U.establish(with_te(good(cp_seid=0x5023), ie(T_TE_ID, b'\x01'), lit)), 'after the delete')
    lit6 = ie(T_LIT, b'\x02' + struct.pack('>H', 39124) + socket.inet_pton(socket.AF_INET6, '::1'))
    r = U.establish(with_te(good(cp_seid=0x5024), ie(T_TE_ID, b'\x01'), lit6))
    s = accepted(r, 'CH=0 IPv6')
    assert created_te(r)[T_LIT] == lit6[4:], created_te(r)[T_LIT].hex()
    U.delete(s)


@test
def t_U10_6_choose_still_works_and_family_is_checked():
    r = U.establish(with_te(good(cp_seid=0x5031), ie(T_TE_ID, b'\x02'), lit_choose()))
    s = accepted(r, 'CH=1 IPv4')
    v = created_te(r)[T_LIT]
    assert v[0] == 0x01 and v[3:7] == socket.inet_aton('127.0.0.31') and len(v) == 7, v.hex()
    U.delete(s)
    r = U.establish(with_te(good(cp_seid=0x5032), ie(T_TE_ID, b'\x02'), lit_choose(False, True)))
    assert r and cause(r) == CAUSE_NO_RESOURCES, 'IPv6 asked from an IPv4 base address: cause %s' % (cause(r) if r else None)
    assert offending(r) == T_LIT


@test
def t_U10_10_traffic_endpoint_id_in_created_te():
    r = U.establish(with_te(good(cp_seid=0x5041), ie(T_TE_ID, b'\x2a'), lit_choose()))
    s = accepted(r, 'with a Traffic Endpoint ID')
    te = created_te(r)
    assert te and te.get(T_TE_ID) == b'\x2a', 'Traffic Endpoint ID in Created Traffic Endpoint: %s' % (te.get(T_TE_ID) if te else None)
    U.delete(s)


@test
def t_U10_6_pool_port_is_returned_once():
    U.restart('{start: 39200, end: 39201}')
    try:
        lit0 = ie(T_LIT, b'\x01' + struct.pack('>H', 39200) + socket.inet_aton('127.0.0.31'))
        # a port the CP function chose never enters the pool, even when it is a pool port
        accepted_and_deleted(U.establish(with_te(good(cp_seid=0x5051), lit0)), 'CH=0 on a pool port')
        a = accepted(U.establish(with_te(good(cp_seid=0x5052), lit_choose())), 'first pool port')
        b = accepted(U.establish(with_te(good(cp_seid=0x5053), lit_choose())), 'second pool port')
        r = U.establish(with_te(good(cp_seid=0x5054), lit_choose()))
        assert r and cause(r) == CAUSE_NO_RESOURCES, 'third: cause %s (pool of two)' % (cause(r) if r else None)
        U.delete(a)
        c = accepted(U.establish(with_te(good(cp_seid=0x5055), lit_choose())), 'port after a delete')
        r = U.establish(with_te(good(cp_seid=0x5056), lit_choose()))
        assert r and cause(r) == CAUSE_NO_RESOURCES, 'fourth: cause %s' % (cause(r) if r else None)
        U.delete(b)
        U.delete(c)
    finally:
        U.restart()


@test
def t_U06_3_fssm_without_mbs_multicast_parameters():
    r = U.establish(replace_ie(good(cp_seid=0x6001), T_CREATE_FAR, far(apply=0x0008, params=False)))
    assert r and cause(r) == CAUSE_CONDITIONAL_MISSING, 'cause %s' % (cause(r) if r else None)
    assert offending(r) == T_MBS_MC_PARAMS, 'offending IE %s' % offending(r)
    # without FSSM the parameters are not required (DROP, FAR for unicast transport only)
    accepted_and_deleted(U.establish(replace_ie(good(cp_seid=0x6002, flags=0x00), T_CREATE_FAR, far(apply=0x0100, params=False))), 'DROP')
    accepted_and_deleted(U.establish(good(cp_seid=0x6003)), 'FSSM with its parameters')


def mc_info(dest=None, src=None):
    parts = []
    if dest is not None:
        parts.append(ie(T_IPMC_ADDR, dest))
    if src is not None:
        parts.append(ie(T_SRC_IP, src))
    return ie(T_IPMC_INFO, parts)


def mc_request(cp_seid, dest=None, src=None, te_id=None, flags=0x01):
    """A request with the multicast addressing in the PDI, or in the Create Traffic Endpoint when te_id is given."""
    info = mc_info(dest, src)
    if te_id is None:
        ies = [node_id(), fseid(cp_seid), pdr(pdi_extra=(info,)), far(), qer(), n4mb_ctrl(flags=flags)]
    else:
        ies = [node_id(), fseid(cp_seid), pdr(pdi_extra=(ie(T_TE_ID, bytes([te_id])),)), far(), qer(),
               ie(T_CREATE_TE, [ie(T_TE_ID, bytes([te_id])), info]), n4mb_ctrl(flags=flags)]
    return ies


G6 = bytes.fromhex('ff3e0000000000000000000000001234')
S6 = bytes.fromhex('20010db8000000000000000000000001')


@test
def t_U03_12_U03_13_ipv6_addressing():
    s = accepted(U.establish(mc_request(0x7001, b'\x01' + G6, b'\x01' + S6)), 'IPv6 group and source')
    assert 'destination [ff3e::1234] source [2001:db8::1]' in U.log_text(), 'IPv6 addressing not read'
    U.delete(s)
    s = accepted(U.establish(mc_request(0x7002, b'\x02' + V4G, b'\x02' + V4S)), 'IPv4 group and source')
    assert 'destination [232.1.1.1] source [10.1.1.1]' in U.log_text(), 'IPv4 addressing not read'
    U.delete(s)
    # source with a prefix length, and a source that is both IPv4 and IPv6
    s = accepted(U.establish(mc_request(0x7003, b'\x02' + V4G, b'\x06' + V4S + b'\x18')), 'source with mask')
    U.delete(s)
    s = accepted(U.establish(mc_request(0x7004, b'\x02' + V4G, b'\x03' + V4S + S6)), 'source IPv4 and IPv6')
    U.delete(s)


@test
def t_U03_12_U03_13_invalid_addressing():
    bad = {
        'multicast, no flag': (b'\x00' + V4G, None),
        'multicast, V4 and V6': (b'\x03' + V4G + G6, None),
        'multicast, A and V4': (b'\x0a' + V4G, None),
        'multicast, A and R': (b'\x0c', None),
        'multicast, V6 flag with an IPv4 address': (b'\x01' + V4G, None),
        'multicast, V4 flag with an IPv6 address': (b'\x02' + G6, None),
        'multicast, range without an end': (b'\x06' + V4G, None),
        'multicast, empty': (b'', None),
        'source, no address flag': (V4G[:0] + b'\x00', None),
        'source, IPv6 flag with an IPv4 address': (None, b'\x01' + V4S),
        'source, mask flag without the octet': (None, b'\x06' + V4S),
        'source, empty': (None, b''),
    }
    for name, (d, sr) in bad.items():
        if name == 'source, no address flag':
            d, sr = b'\x02' + V4G, b'\x00'
        if d is None:
            d = b'\x02' + V4G
        r = U.establish(mc_request(0x7010, d, sr))
        assert r is not None, name + ': no answer (upfd alive: %s)' % U.alive()
        assert cause(r) == CAUSE_MANDATORY_INCORRECT, '%s: cause %s' % (name, cause(r))
        want = T_SRC_IP if name.startswith('source') else T_IPMC_ADDR
        assert offending(r) == want, '%s: offending IE %s, want %d' % (name, offending(r), want)
    accepted_and_deleted(U.establish(good(cp_seid=0x7020)), 'after the rejections')


@test
def t_U03_16_U03_17_traffic_endpoint_addressing():
    s = accepted(U.establish(mc_request(0x7031, b'\x01' + G6, b'\x01' + S6, te_id=9)), 'addressing in the Create Traffic Endpoint')
    assert 'destination [ff3e::1234] source [2001:db8::1]' in U.log_text(), 'Traffic Endpoint addressing not read'
    U.delete(s)
    # a PDR naming a Traffic Endpoint ID that is not in the request
    ies = mc_request(0x7032, b'\x02' + V4G, b'\x02' + V4S, te_id=9)
    ies = replace_ie(ies, T_CREATE_PDR, pdr(pdi_extra=(ie(T_TE_ID, b'\x0a'),)))
    r = U.establish(ies)
    assert r and cause(r) == CAUSE_MANDATORY_INCORRECT, 'cause %s' % (cause(r) if r else None)
    assert offending(r) == T_TE_ID, 'offending IE %s' % offending(r)
    # a PDR naming a Traffic Endpoint ID when the request has no Create Traffic Endpoint
    ies = replace_ie(mc_request(0x7033, b'\x02' + V4G, None), T_CREATE_PDR, pdr(pdi_extra=(ie(T_TE_ID, b'\x09'),)))
    r = U.establish(ies)
    assert r and cause(r) == CAUSE_MANDATORY_INCORRECT, 'no Create Traffic Endpoint: cause %s' % (cause(r) if r else None)
    # an invalid IE in the Create Traffic Endpoint is refused as well
    r = U.establish(mc_request(0x7034, b'\x03' + V4G + G6, None, te_id=9))
    assert r and cause(r) == CAUSE_MANDATORY_INCORRECT and offending(r) == T_IPMC_ADDR, 'invalid IE in the endpoint'
    accepted_and_deleted(U.establish(good(cp_seid=0x7035)), 'after the rejections')


T_CREATED_PDR = 8


def all_ies(r, t):
    return [v for tt, v in r['ies'] if tt == t]


def pdr_request(cp_seid, *pdrs, flags=0x01):
    return [node_id(), fseid(cp_seid), *pdrs, far(), qer(), n4mb_ctrl(flags=flags)]


@test
def t_U10_8_ingress_tunnel_in_the_pdr():
    r = U.establish(pdr_request(0x8001, pdr(pdr_id=1, pdi_extra=(lit_choose(),))))
    s = accepted(r, 'Local Ingress Tunnel in the PDI')
    created = all_ies(r, T_CREATED_PDR)
    assert len(created) == 1, '%d Created PDR' % len(created)
    c = dict(parse_ies(created[0]))
    assert struct.unpack('>H', c[56])[0] == 1, 'PDR ID'
    lit = c.get(T_LIT)
    assert lit and lit[0] == 0x01 and len(lit) == 7 and lit[3:7] == socket.inet_aton('127.0.0.31'), 'LIT %s' % (lit.hex() if lit else None)
    assert find(r, T_CREATED_TE) is None, 'a Created Traffic Endpoint without a Create Traffic Endpoint'
    U.delete(s)
    # two PDRs asking: one tunnel, reported in both Created PDRs
    r = U.establish(pdr_request(0x8002, pdr(pdr_id=1, pdi_extra=(lit_choose(),)), pdr(pdr_id=2, pdi_extra=(lit_choose(),))))
    s = accepted(r, 'two PDRs')
    created = [dict(parse_ies(v)) for v in all_ies(r, T_CREATED_PDR)]
    assert len(created) == 2 and created[0][T_LIT] == created[1][T_LIT], 'one tunnel for both PDRs'
    U.delete(s)
    # the address the CP gave
    want = b'\x01' + struct.pack('>H', 39301) + socket.inet_aton('127.0.0.31')
    r = U.establish(pdr_request(0x8003, pdr(pdr_id=1, pdi_extra=(ie(T_LIT, want),))))
    s = accepted(r, 'CH=0 in the PDI')
    assert dict(parse_ies(all_ies(r, T_CREATED_PDR)[0]))[T_LIT] == want
    U.delete(s)
    # invalid IE in the PDI
    r = U.establish(pdr_request(0x8004, pdr(pdr_id=1, pdi_extra=(ie(T_LIT, b'\x07'),))))
    assert r and cause(r) == CAUSE_MANDATORY_INCORRECT and offending(r) == T_LIT, 'invalid PDI IE: cause %s' % (cause(r) if r else None)
    accepted_and_deleted(U.establish(good(cp_seid=0x8005)), 'after the rejection')


def mc_group_of(r):
    """The N3mb low layer SSM group and the source of the response's Multicast Transport Information."""
    info = dict(parse_ies(find(r, T_N4MB_INFO)))[T_MC_TRANSPORT]
    assert info[5] == 0x04, 'distribution address type/length %02x' % info[5]
    return socket.inet_ntoa(info[6:10])


class N3mbListener:
    """Receives what the UPF sends to the N3mb group."""
    def __init__(self, group):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((group, 2152))
        self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                             socket.inet_aton(group) + socket.inet_aton('127.0.0.1'))
        self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_RECVTOS, 1)
        self.sock.settimeout(0.7)
        self.tos = []

    def packets(self):
        out = []
        try:
            while True:
                data, anc, _, _ = self.sock.recvmsg(65535, 64)
                out.append(data)
                for level, typ, val in anc:
                    if level == socket.IPPROTO_IP and typ == socket.IP_TOS:
                        self.tos.append(val[0])
        except socket.timeout:
            pass
        return out

    def close(self):
        self.sock.close()


def send_multicast(src, dst, port=5000, payload=b'mbs-data'):
    """Sends a UDP packet to a group through the UPF's TUN device, from the given local source address."""
    sk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sk.bind((src, 0))
    sk.sendto(payload, (dst, port))
    sk.close()


def inner_payload_seen(packets, payload=b'mbs-data'):
    return any(payload in p for p in packets)


def data_session(cp_seid, dest, src, flags=0x01):
    r = U.establish(mc_request(cp_seid, dest, src, flags=flags))
    s = accepted(r, 'session for the data path')
    return s, mc_group_of(r)


@test
def t_U11_2_source_is_part_of_the_channel():
    s, group = data_session(0x9001, b'\x02' + V4G, b'\x02' + socket.inet_aton('10.46.0.1'))
    lst = N3mbListener(group)
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        assert inner_payload_seen(lst.packets()), 'data from the session source was not forwarded on N3mb'
        send_multicast('10.46.0.2', '232.1.1.1')
        assert not inner_payload_seen(lst.packets()), 'data from another source was forwarded on N3mb'
    finally:
        lst.close()
        U.delete(s)


@test
def t_U03_15_absent_source_means_any_source():
    s, group = data_session(0x9011, b'\x02' + V4G, None)
    lst = N3mbListener(group)
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        assert inner_payload_seen(lst.packets()), 'data from the first source was not forwarded'
        send_multicast('10.46.0.2', '232.1.1.1')
        assert inner_payload_seen(lst.packets()), 'data from the second source was not forwarded'
    finally:
        lst.close()
        U.delete(s)


@test
def t_U03_14_range_and_any_flags():
    # R: the group range 232.1.1.1 to 232.1.1.9
    s, group = data_session(0x9021, b'\x06' + socket.inet_aton('232.1.1.1') + socket.inet_aton('232.1.1.9'), None)
    lst = N3mbListener(group)
    try:
        send_multicast('10.46.0.1', '232.1.1.5')
        assert inner_payload_seen(lst.packets()), 'group inside the range not forwarded'
        send_multicast('10.46.0.1', '232.1.1.10')
        assert not inner_payload_seen(lst.packets()), 'group outside the range forwarded'
        send_multicast('10.46.0.1', '232.1.1.1')
        assert inner_payload_seen(lst.packets()), 'first group of the range not forwarded'
        send_multicast('10.46.0.1', '232.1.1.9')
        assert inner_payload_seen(lst.packets()), 'last group of the range not forwarded'
    finally:
        lst.close()
        U.delete(s)
    # A: any group, from one source
    s, group = data_session(0x9022, b'\x08', b'\x02' + socket.inet_aton('10.46.0.1'))
    lst = N3mbListener(group)
    try:
        send_multicast('10.46.0.1', '232.7.7.7')
        assert inner_payload_seen(lst.packets()), 'any group, right source: not forwarded'
        send_multicast('10.46.0.2', '232.7.7.7')
        assert not inner_payload_seen(lst.packets()), 'any group, other source: forwarded'
    finally:
        lst.close()
        U.delete(s)
    # a source subnet (mask/prefix length)
    s, group = data_session(0x9023, b'\x02' + V4G, b'\x06' + socket.inet_aton('10.46.0.0') + b'\x1e')
    lst = N3mbListener(group)
    try:
        send_multicast('10.46.0.2', '232.1.1.1')
        assert inner_payload_seen(lst.packets()), 'source inside the /30 not forwarded'
    finally:
        lst.close()
        U.delete(s)


@test
def t_U02_7_area_session_id():
    area = ie(314, b'\x01\x2c')
    s = accepted(U.establish(replace_ie(good(cp_seid=0xa001), T_N4MB_CTRL, n4mb_ctrl(extra=(area,)))), 'with an Area Session ID')
    assert 'Area Session ID [300]' in U.log_text(), 'Area Session ID not read'
    U.delete(s)
    for name, v in (('1 octet', b'\x01'), ('3 octets', b'\x00\x01\x2c'), ('empty', b'')):
        r = U.establish(replace_ie(good(cp_seid=0xa002), T_N4MB_CTRL, n4mb_ctrl(extra=(ie(314, v),))))
        assert r and cause(r) == CAUSE_INVALID_LENGTH, '%s: cause %s' % (name, cause(r) if r else None)
        assert offending(r) == 314, '%s: offending IE %s' % (name, offending(r))
    accepted_and_deleted(U.establish(good(cp_seid=0xa003)), 'after the rejections')


ROUTER = """    multicast_router:
      input_interface: v0
      output_interface: ogstunt
      activate: true
"""


def proc_lines(path):
    return open(path).read().splitlines()


def settled(cond, timeout=2.0):
    """The deletion response is sent before the session is released: wait for the effect."""
    end = time.time() + timeout
    while time.time() < end:
        if cond():
            return True
        time.sleep(0.05)
    return cond()


def igmp_groups():
    """The IPv4 groups joined on v0, as dotted strings, from /proc/net/igmp."""
    out, dev = set(), None
    for line in proc_lines('/proc/net/igmp')[1:]:
        if not line.startswith('\t'):
            dev = line.split()[1].rstrip(':')
        elif dev == 'v0':
            out.add(socket.inet_ntoa(bytes.fromhex(line.split()[0])[::-1]))
    return out


def igmp_sources():
    """(group, source) pairs of the source filters on v0, from /proc/net/mcfilter."""
    out = set()
    for line in proc_lines('/proc/net/mcfilter')[1:]:
        f = line.split()
        if len(f) > 3 and f[1] == 'v0':
            out.add((socket.inet_ntoa(struct.pack('>I', int(f[2], 16))), socket.inet_ntoa(struct.pack('>I', int(f[3], 16)))))
    return out


def mld_groups():
    out = set()
    for line in proc_lines('/proc/net/igmp6'):
        f = line.split()
        if len(f) > 2 and f[1] == 'v0':
            out.add(socket.inet_ntop(socket.AF_INET6, bytes.fromhex(f[2])))
    return out


def mld_sources():
    out = set()
    for line in proc_lines('/proc/net/mcfilter6'):
        f = line.split()
        if len(f) > 3 and f[1] == 'v0':
            out.add((socket.inet_ntop(socket.AF_INET6, bytes.fromhex(f[2])), socket.inet_ntop(socket.AF_INET6, bytes.fromhex(f[3]))))
    return out


@test
def t_U04_2_U04_3_U04_6_join_leave_ipv4():
    U.restart('0', ROUTER)
    try:
        # JMBSSM clear: the router is active, nothing is joined
        s = accepted(U.establish(mc_request(0xb001, b'\x02' + V4G, b'\x02' + V4S, flags=0x01)), 'JMBSSM clear')
        assert '232.1.1.1' not in igmp_groups(), 'joined without JMBSSM'
        U.delete(s)
        # JMBSSM set: the channel is joined, source specific
        s = accepted(U.establish(mc_request(0xb002, b'\x02' + V4G, b'\x02' + V4S, flags=0x03)), 'JMBSSM set')
        assert '232.1.1.1' in igmp_groups(), 'group not joined: %s' % igmp_groups()
        assert ('232.1.1.1', '10.1.1.1') in igmp_sources(), 'join is not source specific: %s' % igmp_sources()
        # a second session on the same channel shares the join
        s2 = accepted(U.establish(mc_request(0xb003, b'\x02' + V4G, b'\x02' + V4S, flags=0x03)), 'second session')
        U.delete(s)
        time.sleep(0.3)
        assert ('232.1.1.1', '10.1.1.1') in igmp_sources(), 'left while a session still uses the channel'
        U.delete(s2)
        assert settled(lambda: '232.1.1.1' not in igmp_groups() and not igmp_sources()), 'channel kept after the sessions ended: %s %s' % (igmp_groups(), igmp_sources())
        # no source address: any source
        s = accepted(U.establish(mc_request(0xb004, b'\x02' + V4G, None, flags=0x03)), 'any source')
        assert '232.1.1.1' in igmp_groups(), 'group not joined for any source'
        assert not igmp_sources(), 'a source filter for an any-source join: %s' % igmp_sources()
        U.delete(s)
        assert settled(lambda: '232.1.1.1' not in igmp_groups()), 'any-source group kept'
        # a rejected establishment leaves nothing joined
        r = U.establish(replace_ie(mc_request(0xb005, b'\x02' + V4G, b'\x02' + V4S, flags=0x03), T_CREATE_FAR, far(apply=0x0008, params=False)))
        assert r and cause(r) != CAUSE_ACCEPTED
        assert '232.1.1.1' not in igmp_groups(), 'joined by a rejected request'
    finally:
        U.restart()


@test
def t_U04_5_join_failures_are_reported():
    # router not active (the default configuration)
    r = U.establish(mc_request(0xb011, b'\x02' + V4G, b'\x02' + V4S, flags=0x03))
    assert r and cause(r) == CAUSE_SERVICE_NOT_SUPPORTED, 'router inactive: cause %s' % (cause(r) if r else None)
    U.restart('0', ROUTER)
    try:
        r = U.establish(good(0xb012, flags=0x03))
        assert r and cause(r) == CAUSE_CONDITIONAL_MISSING and offending(r) == T_IPMC_INFO, 'no address: cause %s' % (cause(r) if r else None)
        r = U.establish(mc_request(0xb013, b'\x02' + socket.inet_aton('10.1.1.9'), b'\x02' + V4S, flags=0x03))
        assert r and cause(r) == CAUSE_MANDATORY_INCORRECT and offending(r) == T_IPMC_ADDR, 'unicast group: cause %s' % (cause(r) if r else None)
        r = U.establish(mc_request(0xb014, b'\x06' + socket.inet_aton('232.1.1.1') + socket.inet_aton('232.1.1.9'), None, flags=0x03))
        assert r and cause(r) == CAUSE_SERVICE_NOT_SUPPORTED, 'range: cause %s' % (cause(r) if r else None)
        r = U.establish(mc_request(0xb015, b'\x02' + V4G, b'\x01' + S6, flags=0x03))
        assert r and cause(r) == CAUSE_MANDATORY_INCORRECT and offending(r) == T_SRC_IP, 'family mismatch: cause %s' % (cause(r) if r else None)
        # a failing system call: the kernel limit of group memberships on the router's socket
        subprocess.run(['sysctl', '-q', '-w', 'net.ipv4.igmp_max_memberships=1'], check=True)
        a = accepted(U.establish(mc_request(0xb016, b'\x02' + socket.inet_aton('232.2.2.1'), None, flags=0x03)), 'first channel')
        r = U.establish(mc_request(0xb017, b'\x02' + socket.inet_aton('232.2.2.2'), None, flags=0x03))
        assert r and cause(r) == CAUSE_SYSTEM_FAILURE, 'membership limit: cause %s' % (cause(r) if r else None)
        assert '232.2.2.2' not in igmp_groups(), 'a failed join left the group'
        U.delete(a)
        assert settled(lambda: not igmp_groups() - {'224.0.0.1'}), 'groups left: %s' % igmp_groups()
        accepted_and_deleted(U.establish(mc_request(0xb018, b'\x02' + socket.inet_aton('232.2.2.2'), None, flags=0x03)), 'after the limit')
    finally:
        subprocess.run(['sysctl', '-q', '-w', 'net.ipv4.igmp_max_memberships=20'])
        U.restart()


@test
def t_U04_4_join_leave_ipv6():
    U.restart('0', ROUTER)
    try:
        s = accepted(U.establish(mc_request(0xb021, b'\x01' + G6, b'\x01' + S6, flags=0x03)), 'IPv6 JMBSSM')
        assert 'ff3e::1234' in mld_groups(), 'group not joined: %s' % mld_groups()
        assert ('ff3e::1234', '2001:db8::1') in mld_sources(), 'IPv6 join not source specific: %s' % mld_sources()
        U.delete(s)
        assert settled(lambda: 'ff3e::1234' not in mld_groups() and not mld_sources()), 'IPv6 channel kept'
        s = accepted(U.establish(mc_request(0xb022, b'\x01' + G6, None, flags=0x03)), 'IPv6 any source')
        assert 'ff3e::1234' in mld_groups(), 'IPv6 group not joined for any source'
        U.delete(s)
        assert settled(lambda: 'ff3e::1234' not in mld_groups()), 'IPv6 group kept'
    finally:
        U.restart()


def routed_sender(src='10.99.0.2', dst='232.1.1.1', payload=b'mbs-data'):
    """Sends from the far end of the veth pair, so the packet is received on the router's input interface."""
    sk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sk.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 8)
    sk.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(src))
    sk.bind((src, 0))
    sk.sendto(payload, (dst, 5000))
    sk.close()


@test
def t_U04_data_from_the_joined_channel_reaches_n3mb():
    U.restart('0', ROUTER)
    try:
        subprocess.run(['ip', 'route', 'add', '232.0.0.0/8', 'dev', 'v1'], stderr=subprocess.DEVNULL)
        src = socket.inet_aton('10.99.0.2')
        r = U.establish(mc_request(0xb032, b'\x02' + V4G, b'\x02' + src, flags=0x03))
        s = accepted(r, 'source specific session')
        lst = N3mbListener(mc_group_of(r))
        try:
            routed_sender()
            assert inner_payload_seen(lst.packets()), 'data of the joined channel did not reach N3mb'
        finally:
            lst.close()
        U.delete(s)
    finally:
        U.restart()


@test
def t_U06_11_drop_drops():
    # DROP together with the multicast parameters: the FAR has a destination, the packet must still be dropped
    r = U.establish(replace_ie(mc_request(0xc001, b'\x02' + V4G, None), T_CREATE_FAR, far(apply=0x0100)))
    s = accepted(r, 'DROP FAR')
    lst = N3mbListener(mc_group_of(r))
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        assert not inner_payload_seen(lst.packets()), 'a FAR with DROP forwarded'
        assert 'Not implemented =' not in U.log_text(), 'DROP reported as not implemented'
    finally:
        lst.close()
        U.delete(s)
    # without DROP the same request forwards
    r = U.establish(mc_request(0xc002, b'\x02' + V4G, None))
    s = accepted(r, 'FSSM FAR')
    lst = N3mbListener(mc_group_of(r))
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        assert inner_payload_seen(lst.packets()), 'the FSSM FAR did not forward'
    finally:
        lst.close()
        U.delete(s)


@test
def t_U06_5_transport_level_marking_and_network_instance():
    tlm = ie(30, b'\xb8\xfc')
    r = U.establish(replace_ie(mc_request(0xc011, b'\x02' + V4G, None), T_CREATE_FAR, far(mc_extra=(tlm,))))
    s = accepted(r, 'with a Transport Level Marking')
    lst = N3mbListener(mc_group_of(r))
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        assert inner_payload_seen(lst.packets()), 'nothing forwarded'
        assert lst.tos and all(t & 0xfc == 0xb8 for t in lst.tos), 'TOS of the N3mb packets: %s' % lst.tos
    finally:
        lst.close()
        U.delete(s)
    # a session without the IE is not marked, also after one that was (the socket is shared)
    r = U.establish(mc_request(0xc012, b'\x02' + V4G, None))
    s = accepted(r, 'without')
    lst = N3mbListener(mc_group_of(r))
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        assert inner_payload_seen(lst.packets())
        assert lst.tos and all(t & 0xfc == 0 for t in lst.tos), 'TOS without a marking: %s' % lst.tos
    finally:
        lst.close()
        U.delete(s)
    # a Transport Level Marking of one octet
    r = U.establish(replace_ie(mc_request(0xc013, b'\x02' + V4G, None), T_CREATE_FAR, far(mc_extra=(ie(30, b'\xb8'),))))
    assert r and cause(r) == CAUSE_INVALID_LENGTH and offending(r) == 30, 'short IE: cause %s' % (cause(r) if r else None)
    # a Network Instance is accepted
    ni = ie(T_NET_INST, b'\x03mbs')
    accepted_and_deleted(U.establish(replace_ie(mc_request(0xc014, b'\x02' + V4G, None), T_CREATE_FAR, far(mc_extra=(ni,)))), 'with a Network Instance')


@test
def t_U13_4_closed_downlink_gate_discards():
    # DL gate (bits 2 to 1) closed = 1
    r = U.establish(replace_ie(mc_request(0xd001, b'\x02' + V4G, None), T_CREATE_QER, qer(gate=0x01)))
    s = accepted(r, 'closed DL gate')
    lst = N3mbListener(mc_group_of(r))
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        assert not inner_payload_seen(lst.packets()), 'data forwarded through a closed downlink gate'
    finally:
        lst.close()
        U.delete(s)
    # only the uplink gate closed (bits 4 to 3 = 1): downlink traffic flows
    r = U.establish(replace_ie(mc_request(0xd002, b'\x02' + V4G, None), T_CREATE_QER, qer(gate=0x04)))
    s = accepted(r, 'closed UL gate')
    lst = N3mbListener(mc_group_of(r))
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        assert inner_payload_seen(lst.packets()), 'an open downlink gate did not forward'
    finally:
        lst.close()
        U.delete(s)


def gtpu_parse(p):
    """The GTP-U header (TS 29.281 cl. 5.1) and the extension headers of a G-PDU, and the T-PDU."""
    flags, mtype, length, teid = p[0], p[1], struct.unpack('>H', p[2:4])[0], struct.unpack('>I', p[4:8])[0]
    out = dict(flags=flags, type=mtype, length=length, teid=teid, ext=[])
    pos = 8
    if flags & 0x07:
        out['seq'], out['npdu'], nxt = struct.unpack('>H', p[8:10])[0], p[10], p[11]
        pos = 12
        while flags & 0x04 and nxt:
            n = p[pos] * 4
            out['ext'].append((nxt, p[pos + 1:pos + n - 1]))
            nxt = p[pos + n - 1]
            pos += n
    out['tpdu'] = p[pos:]
    return out


def c_teid_of(r):
    info = dict(parse_ies(find(r, T_N4MB_INFO)))[T_MC_TRANSPORT]
    return struct.unpack('>I', info[1:5])[0]


@test
def t_U12_6_U12_8_gpdu_header_and_pdu_session_container():
    r = U.establish(mc_request(0xe001, b'\x02' + V4G, None))
    s = accepted(r, 'session')
    lst = N3mbListener(mc_group_of(r))
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        pk = lst.packets()
        assert pk, 'nothing on N3mb'
        g = gtpu_parse(pk[0])
        assert g['flags'] == 0x34, 'flags %02x: version 1, PT 1, spare 0, E 1, S 0, PN 0 expected 0x34' % g['flags']
        assert g['type'] == 255, 'message type %d' % g['type']
        assert g['length'] == len(pk[0]) - 8, 'length %d for %d octets' % (g['length'], len(pk[0]))
        assert g['teid'] == c_teid_of(r), 'TEID %08x, C-TEID of the response %08x' % (g['teid'], c_teid_of(r))
        assert g['ext'] == [(0x85, b'\x00\x01')], 'extension headers %s' % g['ext']
        # the extension: length 1 (4 octets) = PDU Type 0 with QMP, SNP, MSNP clear, then PPP, RQI and QFI 1
        assert b'mbs-data' in g['tpdu'] and g['tpdu'][0] >> 4 == 4, 'T-PDU is not the IPv4 packet'
    finally:
        lst.close()
        U.delete(s)


@test
def t_U12_10_destination_port_is_2152():
    U.restart('0', '', gtpu_port=2153)
    try:
        r = U.establish(mc_request(0xe011, b'\x02' + V4G, None))
        s = accepted(r, 'session, UPF listening on 2153')
        lst = N3mbListener(mc_group_of(r))
        try:
            send_multicast('10.46.0.1', '232.1.1.1')
            assert inner_payload_seen(lst.packets()), 'the N3mb group did not receive on 2152'
        finally:
            lst.close()
        U.delete(s)
    finally:
        U.restart()


@test
def t_U13_2_U12_9_U13_3_dl_mbs_qfi_sequence_number():
    ies = replace_ie(mc_request(0xe021, b'\x02' + V4G, None), T_CREATE_QER, qer(indications=0x01))
    r = U.establish(ies)
    s = accepted(r, 'IQFISN')
    lst = N3mbListener(mc_group_of(r))
    try:
        for _ in range(3):
            send_multicast('10.46.0.1', '232.1.1.1')
            time.sleep(0.05)
        pk = [gtpu_parse(p) for p in lst.packets()]
        assert len(pk) == 3, '%d packets' % len(pk)
        seqs = []
        for g in pk:
            assert len(g['ext']) == 1 and g['ext'][0][0] == 0x85, 'extension headers %s' % g['ext']
            c = g['ext'][0][1]
            # extension header length 2: PDU Type 0 with MSNP (bit 1), PPP/RQI/QFI, the 4 octets, next type
            assert len(c) == 6 and c[0] == 0x02 and c[1] == 0x01, 'container %s' % c.hex()
            seqs.append(struct.unpack('>I', c[2:6])[0])
        assert seqs == [0, 1, 2], 'sequence numbers %s' % seqs
    finally:
        lst.close()
        U.delete(s)
    # another session, its own flow: its number starts at 0 again, and without IQFISN there is none
    ies = replace_ie(mc_request(0xe022, b'\x02' + V4G, None), T_CREATE_QER, qer(indications=0x01, qfi=2))
    r = U.establish(ies)
    s = accepted(r, 'second flow')
    lst = N3mbListener(mc_group_of(r))
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        g = gtpu_parse(lst.packets()[0])
        assert struct.unpack('>I', g['ext'][0][1][2:6])[0] == 0 and g['ext'][0][1][1] == 0x02, 'second flow container %s' % g['ext'][0][1].hex()
    finally:
        lst.close()
        U.delete(s)


@test
def t_tmgi_plmn_octets_of_ts_24_008():
    # PLMN 001/01 is 00 f1 10 and PLMN 310/014 is 13 40 10 in figure 10.5.154 (octet 6: MCC2 MCC1, octet 7: MNC3 MCC3,
    # octet 8: MNC2 MNC1)
    for plmn, want, seid in ((b'\x00\xf1\x10', 'TMGI PLMN [001/01]', 0xf001), (b'\x13\x40\x10', 'TMGI PLMN [310/014]', 0xf002),
                            (b'\x62\xf2\x20', 'TMGI PLMN [262/02]', 0xf003)):
        s = accepted(U.establish(good(cp_seid=seid)[:-1] + [n4mb_ctrl(tmgi_id(plmn=plmn))]), want)
        assert want in U.log_text(), '%s not read from %s' % (want, plmn.hex())
        U.delete(s)


@test
def t_create_qer_without_gate_status_reports_gate_status():
    # TS 29.244 Table 7.5.2.5-1: Gate Status is M; the Offending IE is its own type (25), not Apply Action (44)
    r = U.establish(replace_ie(good(cp_seid=0xd001), T_CREATE_QER, qer(drop=(T_GATE,))))
    assert r and cause(r) == CAUSE_MANDATORY_MISSING, 'cause %s' % (cause(r) if r else None)
    assert offending(r) == T_GATE, 'offending IE %s, expected %d' % (offending(r), T_GATE)
    accepted_and_deleted(U.establish(good(cp_seid=0xd002)), 'after the rejection')


# ---- N4mb Session Modification (TS 29.244 cl. 7.5.4) ----------------------------------------------------------

APPLY_DROP, APPLY_FSSM, APPLY_MBSU, APPLY_BUFF = 0x0100, 0x0008, 0x0010, 0x0004


def update_far(far_id=1, apply=None, extra=(), drop=()):
    parts = [ie(T_FAR_ID, struct.pack('>I', far_id))]
    if apply is not None:
        parts.append(ie(T_APPLY, struct.pack('>H', apply)))
    parts += list(extra)
    parts = [p for p in parts if struct.unpack('>H', p[:2])[0] not in drop]
    return ie(T_UPDATE_FAR, parts)


def modify(seid, *ies):
    return U.request(MODIFICATION_REQUEST, list(ies), seid=seid)


def drop_session(cp_seid, flags=0x01, params=True):
    """A session whose FAR drops (the unicast-only start of TS 29.244 cl. 5.34.2.2), with the multicast parameters."""
    r = U.establish(replace_ie(mc_request(cp_seid, b'\x02' + V4G, None, flags=flags), T_CREATE_FAR,
                               far(apply=APPLY_DROP, params=params)))
    return r, accepted(r, 'DROP session')


def forwards(r, cp_seid_unused=None):
    lst = N3mbListener(mc_group_of(r))
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        return inner_payload_seen(lst.packets())
    finally:
        lst.close()


@test
def t_U08_1_modification_of_an_mbs_session_is_processed():
    r, s = drop_session(0xe101)
    try:
        m = modify(s, update_far(apply=APPLY_DROP))
        assert m and m['type'] == 53, 'no Session Modification Response'
        assert cause(m) == CAUSE_ACCEPTED, 'cause %s offending %s' % (cause(m), offending(m))
        assert m['seid'] == 0xe101, 'response header SEID %x, the CP SEID is e101' % (m['seid'] or 0)
        # an unknown SEID is still answered "Session Context Not Found"
        m = modify(s + 0x1000, update_far(apply=APPLY_DROP))
        assert m and cause(m) == CAUSE_CONTEXT_NOT_FOUND, 'unknown SEID: cause %s' % (cause(m) if m else None)
    finally:
        U.delete(s)


@test
def t_U08_2_U06_13_update_far_changes_the_apply_action():
    r, s = drop_session(0xe111)
    try:
        assert not forwards(r), 'DROP forwarded'
        m = modify(s, update_far(apply=APPLY_FSSM))
        assert m and cause(m) == CAUSE_ACCEPTED, 'FSSM: cause %s offending %s' % (cause(m) if m else None, offending(m) if m else None)
        assert forwards(r), 'nothing on N3mb after the Update FAR with FSSM'
        m = modify(s, update_far(apply=APPLY_DROP))
        assert m and cause(m) == CAUSE_ACCEPTED
        assert not forwards(r), 'forwarded after the Update FAR back to DROP'
        m = modify(s, update_far(apply=APPLY_BUFF))
        assert m and cause(m) == CAUSE_ACCEPTED
        assert not forwards(r), 'forwarded while buffering'
        m = modify(s, update_far(apply=APPLY_FSSM))
        assert m and cause(m) == CAUSE_ACCEPTED
        assert forwards(r), 'not forwarding after the reactivation'
    finally:
        U.delete(s)


@test
def t_U08_modification_that_is_not_valid_changes_nothing():
    r, s = drop_session(0xe121)
    try:
        # FAR ID (M) missing
        m = modify(s, update_far(apply=APPLY_FSSM, drop=(T_FAR_ID,)))
        assert m and cause(m) == CAUSE_MANDATORY_MISSING and offending(m) == T_FAR_ID, 'no FAR ID: %s/%s' % (cause(m), offending(m))
        # unknown FAR ID
        m = modify(s, update_far(far_id=99, apply=APPLY_FSSM))
        assert m and cause(m) == CAUSE_MANDATORY_INCORRECT and offending(m) == T_FAR_ID, 'FAR 99: %s/%s' % (cause(m), offending(m))
        # a second Update FAR that fails: the first one is not applied either
        m = modify(s, update_far(apply=APPLY_FSSM), update_far(far_id=99, apply=APPLY_FSSM))
        assert m and cause(m) == CAUSE_MANDATORY_INCORRECT, 'cause %s' % (cause(m) if m else None)
        assert not forwards(r), 'a rejected request changed the FAR'
        # an IE of the N4mb request that the MB-UPF does not process is rejected, not skipped
        m = modify(s, update_far(apply=APPLY_FSSM), ie(6, [ie(81, struct.pack('>I', 9))]))
        assert m and cause(m) == CAUSE_SERVICE_NOT_SUPPORTED and offending(m) == 6, 'Create URR: %s/%s' % (cause(m), offending(m))
        assert not forwards(r), 'a rejected request changed the FAR'
    finally:
        U.delete(s)
    # FSSM for a FAR created without MBS Multicast Parameters: the parameters are conditional in the Create FAR
    r, s = drop_session(0xe122, params=False)
    try:
        m = modify(s, update_far(apply=APPLY_FSSM))
        assert m and cause(m) == CAUSE_CONDITIONAL_MISSING and offending(m) == T_MBS_MC_PARAMS, '%s/%s' % (cause(m), offending(m))
    finally:
        U.delete(s)
    # FSSM when the session has no low layer SSM (PLLSSM was not requested)
    r, s = drop_session(0xe123, flags=0x00)
    try:
        m = modify(s, update_far(apply=APPLY_FSSM))
        assert m and cause(m) == CAUSE_MANDATORY_INCORRECT and offending(m) == T_APPLY, '%s/%s' % (cause(m), offending(m))
    finally:
        U.delete(s)


def add_unicast(uid, teid=0x1000, ip='127.0.0.41', dst_if=0, ohc=None, drop=(), extra=()):
    """Add MBS Unicast Parameters (Table 7.5.2.3-6): a GTP-U/UDP/IPv4 outer header toward a remote peer."""
    if ohc is None:
        ohc = b'\x01\x00' + struct.pack('>I', teid) + socket.inet_aton(ip)
    parts = [ie(T_DST_IF, bytes([dst_if])), ie(T_UNICAST_ID, struct.pack('>H', uid)), ie(T_OHC, ohc)]
    parts = [p for p in parts if struct.unpack('>H', p[:2])[0] not in drop]
    return ie(T_ADD_UNICAST, parts + list(extra))


def remove_unicast(uid, drop=()):
    parts = [] if T_UNICAST_ID in drop else [ie(T_UNICAST_ID, struct.pack('>H', uid))]
    return ie(T_REMOVE_UNICAST, parts)


@test
def t_U08_3_U08_6_add_and_remove_mbs_unicast_parameters():
    r, s = drop_session(0xe201)
    try:
        def ok(what, *ies):
            m = modify(s, *ies)
            assert m and cause(m) == CAUSE_ACCEPTED, '%s: cause %s offending %s' % (what, cause(m) if m else None, offending(m) if m else None)

        def bad(what, cause_want, off_want, *ies):
            m = modify(s, *ies)
            assert m and cause(m) == cause_want and offending(m) == off_want, '%s: cause %s offending %s, wanted %s/%s' % (
                what, cause(m) if m else None, offending(m) if m else None, cause_want, off_want)

        ok('one peer', update_far(extra=(add_unicast(1),)))
        ok('several peers', update_far(extra=(add_unicast(2, 0x2000, '127.0.0.42'), add_unicast(3, 0x3000, '127.0.0.43'))))
        bad('the same ID again', CAUSE_MANDATORY_INCORRECT, T_UNICAST_ID, update_far(extra=(add_unicast(1),)))
        bad('the same ID twice in a request', CAUSE_MANDATORY_INCORRECT, T_UNICAST_ID,
            update_far(extra=(add_unicast(4), add_unicast(4))))
        # the first ID 4 of the request was undone with the request
        ok('ID 4 after the failed request', update_far(extra=(add_unicast(4),)))
        bad('remove of an ID that is not provisioned', CAUSE_MANDATORY_INCORRECT, T_UNICAST_ID,
            update_far(extra=(remove_unicast(77),)))
        ok('remove', update_far(extra=(remove_unicast(1),)))
        bad('remove again', CAUSE_MANDATORY_INCORRECT, T_UNICAST_ID, update_far(extra=(remove_unicast(1),)))
        ok('the ID is free again', update_far(extra=(add_unicast(1),)))
        # a failed request puts a removed peer back
        bad('remove then fail', CAUSE_MANDATORY_INCORRECT, T_UNICAST_ID,
            update_far(extra=(remove_unicast(2), remove_unicast(88))))
        bad('ID 2 is still provisioned', CAUSE_MANDATORY_INCORRECT, T_UNICAST_ID, update_far(extra=(add_unicast(2),)))
        # removal and addition of one ID in one request: removals first
        ok('replace ID 3', update_far(extra=(add_unicast(3, 0x3333, '127.0.0.44'), remove_unicast(3))))
        # content of the IE
        bad('no Destination Interface', CAUSE_MANDATORY_MISSING, T_DST_IF, update_far(extra=(add_unicast(5, drop=(T_DST_IF,)),)))
        bad('no ID', CAUSE_MANDATORY_MISSING, T_UNICAST_ID, update_far(extra=(add_unicast(5, drop=(T_UNICAST_ID,)),)))
        bad('no Outer Header Creation', CAUSE_MANDATORY_MISSING, T_OHC, update_far(extra=(add_unicast(5, drop=(T_OHC,)),)))
        bad('Outer Header Creation that is not GTP-U', CAUSE_MANDATORY_INCORRECT, T_OHC,
            update_far(extra=(add_unicast(5, ohc=b'\x00\x00'),)))
        bad('UDP/IPv4 outer header', CAUSE_SERVICE_NOT_SUPPORTED, T_OHC,
            update_far(extra=(add_unicast(5, ohc=b'\x05\x00' + struct.pack('>I', 1) + socket.inet_aton('127.0.0.45')),)))
        bad('Outer Header Creation too short', CAUSE_INVALID_LENGTH, T_OHC,
            update_far(extra=(add_unicast(5, ohc=b'\x01\x00\x00\x00\x10\x00'),)))
        bad('ID of the wrong length', CAUSE_INVALID_LENGTH, T_UNICAST_ID,
            update_far(extra=(ie(T_ADD_UNICAST, [ie(T_DST_IF, b'\x00'), ie(T_UNICAST_ID, b'\x01'),
                                                 ie(T_OHC, b'\x01\x00' + struct.pack('>I', 1) + socket.inet_aton('127.0.0.45'))]),)))
        bad('Transport Level Marking of the wrong length', CAUSE_INVALID_LENGTH, T_TLM,
            update_far(extra=(add_unicast(5, extra=(ie(T_TLM, b'\xb8'),)),)))
        # an IPv6 peer when the UPF has no IPv6 GTP-U socket (the test configuration): not reachable, the request fails
        bad('IPv6 peer without an IPv6 GTP-U socket', CAUSE_SYSTEM_FAILURE, T_OHC, update_far(extra=(
            add_unicast(6, ohc=b'\x02\x00' + struct.pack('>I', 6) + bytes.fromhex('fd000000000000000000000000000041')),)))
        ok('ID 6 was not kept', update_far(extra=(add_unicast(6),)))
        ok('with a Transport Level Marking and a Network Instance', update_far(extra=(
            add_unicast(7, extra=(ie(T_TLM, b'\xb8\xfc'), ie(T_NET_INST, b'\x03n3a')) ),)))
    finally:
        U.delete(s)


@test
def t_U08_3_add_mbs_unicast_parameters_in_the_create_far():
    ies = replace_ie(mc_request(0xe211, b'\x02' + V4G, None), T_CREATE_FAR,
                     far(apply=APPLY_FSSM | APPLY_MBSU, mc_extra=()))
    # the Add IEs are children of the Create FAR: rebuild it with two peers
    cfar = ie(T_CREATE_FAR, [ie(T_FAR_ID, struct.pack('>I', 1)), ie(T_APPLY, struct.pack('>H', APPLY_FSSM | APPLY_MBSU)),
                             ie(T_MBS_MC_PARAMS, [ie(T_DST_IF, b'\x00'), ie(T_OHC, b'\x00\x04')]),
                             add_unicast(1), add_unicast(2, 0x2000, '127.0.0.42')])
    r = U.establish(replace_ie(ies, T_CREATE_FAR, cfar))
    s = accepted(r, 'Create FAR with two Add MBS Unicast Parameters')
    try:
        m = modify(s, update_far(extra=(add_unicast(2),)))
        assert m and cause(m) == CAUSE_MANDATORY_INCORRECT and offending(m) == T_UNICAST_ID, 'ID 2 of the Create FAR is not provisioned'
        m = modify(s, update_far(extra=(remove_unicast(1), remove_unicast(2))))
        assert m and cause(m) == CAUSE_ACCEPTED
    finally:
        U.delete(s)
    # an invalid Add in the Create FAR rejects the establishment with the Offending IE above 255
    cfar = ie(T_CREATE_FAR, [ie(T_FAR_ID, struct.pack('>I', 1)), ie(T_APPLY, struct.pack('>H', APPLY_MBSU)),
                             add_unicast(1, drop=(T_UNICAST_ID,))])
    r = U.establish(replace_ie(mc_request(0xe212, b'\x02' + V4G, None), T_CREATE_FAR, cfar))
    assert r and cause(r) == CAUSE_MANDATORY_MISSING and offending(r) == T_UNICAST_ID, 'cause %s offending %s' % (
        cause(r) if r else None, offending(r) if r else None)
    accepted_and_deleted(U.establish(good(cp_seid=0xe213)), 'after the rejection')


class GtpuPeer:
    """A remote GTP-U peer (an NG-RAN node) of unicast N3mb transport: receives what the UPF sends to ip:2152."""
    def __init__(self, ip):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_RECVTOS, 1)
        self.sock.bind((ip, 2152))
        self.sock.settimeout(0.7)
        self.tos = []

    packets = N3mbListener.packets

    def close(self):
        self.sock.close()


def sent_to(r, *peers):
    """Sends one packet to the session's group and returns, for each peer, the GTP-U packets it received, then the
    ones of the N3mb group (when the session has one)."""
    lst = N3mbListener(mc_group_of(r)) if find(r, T_N4MB_INFO) else None
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        got = [p.packets() for p in peers]
        return got, (lst.packets() if lst else [])
    finally:
        if lst:
            lst.close()


@test
def t_U08_4_U08_5_U12_5_mbsu_forwards_to_every_peer():
    r, s = drop_session(0xe301)
    peers = [GtpuPeer('127.0.0.41'), GtpuPeer('127.0.0.42'), GtpuPeer('127.0.0.43')]
    try:
        got, grp = sent_to(r, *peers)
        assert not any(got) and not grp, 'DROP forwarded'
        m = modify(s, update_far(apply=APPLY_MBSU, extra=(add_unicast(1, 0x1111, '127.0.0.41'),)))
        assert m and cause(m) == CAUSE_ACCEPTED, 'MBSU, one peer: %s/%s' % (cause(m) if m else None, offending(m) if m else None)
        got, grp = sent_to(r, *peers)
        assert len(got[0]) == 1 and not got[1] and not got[2], 'one peer: %s' % [len(x) for x in got]
        assert not grp, 'MBSU without FSSM sent to the N3mb group'
        g = gtpu_parse(got[0][0])
        assert g['flags'] == 0x34 and g['type'] == 255, 'header %02x/%d' % (g['flags'], g['type'])
        assert g['teid'] == 0x1111, 'TEID %08x, the peer gave 1111' % g['teid']
        assert g['ext'] == [(0x85, b'\x00\x01')], 'extension headers %s' % g['ext']
        assert b'mbs-data' in g['tpdu'] and g['tpdu'][0] >> 4 == 4, 'T-PDU is not the IPv4 packet'
        # more peers: each gets its own copy, with its own TEID
        m = modify(s, update_far(extra=(add_unicast(2, 0x2222, '127.0.0.42'), add_unicast(3, 0x3333, '127.0.0.43'))))
        assert m and cause(m) == CAUSE_ACCEPTED
        got, grp = sent_to(r, *peers)
        assert [len(x) for x in got] == [1, 1, 1], 'copies per peer %s' % [len(x) for x in got]
        assert [gtpu_parse(x[0])['teid'] for x in got] == [0x1111, 0x2222, 0x3333], 'TEIDs'
        assert all(b'mbs-data' in gtpu_parse(x[0])['tpdu'] for x in got), 'payload differs between the copies'
        # a removed peer gets nothing; the others still do
        m = modify(s, update_far(extra=(remove_unicast(2),)))
        assert m and cause(m) == CAUSE_ACCEPTED
        got, grp = sent_to(r, *peers)
        assert [len(x) for x in got] == [1, 0, 1], 'after Remove: %s' % [len(x) for x in got]
        # back to DROP: nothing; the peers stay provisioned, so MBSU again needs no Add
        assert modify(s, update_far(apply=APPLY_DROP)) and True
        got, grp = sent_to(r, *peers)
        assert not any(got), 'DROP forwarded to peers'
        m = modify(s, update_far(apply=APPLY_MBSU))
        assert m and cause(m) == CAUSE_ACCEPTED, 'MBSU with the peers kept: %s' % (cause(m) if m else None)
        got, grp = sent_to(r, *peers)
        assert [len(x) for x in got] == [1, 0, 1], 'reactivation: %s' % [len(x) for x in got]
    finally:
        for p in peers:
            p.close()
        U.delete(s)


@test
def t_U06_12_fssm_and_mbsu_together():
    ies = replace_ie(mc_request(0xe311, b'\x02' + V4G, None), T_CREATE_QER, qer(indications=0x01))
    r = U.establish(replace_ie(ies, T_CREATE_FAR, far(apply=APPLY_DROP)))
    s = accepted(r, 'session')
    peers = [GtpuPeer('127.0.0.41'), GtpuPeer('127.0.0.42')]
    try:
        m = modify(s, update_far(apply=APPLY_FSSM | APPLY_MBSU,
                                 extra=(add_unicast(1, 0x1111, '127.0.0.41'), add_unicast(2, 0x2222, '127.0.0.42'))))
        assert m and cause(m) == CAUSE_ACCEPTED, '%s/%s' % (cause(m) if m else None, offending(m) if m else None)
        for n in range(2):
            got, grp = sent_to(r, *peers)
            assert [len(x) for x in got] == [1, 1] and len(grp) == 1, 'peers %s, group %d' % ([len(x) for x in got], len(grp))
            allp = [gtpu_parse(grp[0])] + [gtpu_parse(x[0]) for x in got]
            assert [g['teid'] for g in allp] == [c_teid_of(r), 0x1111, 0x2222], 'TEIDs %s' % [hex(g['teid']) for g in allp]
            # the DL MBS QFI Sequence Number is that of the packet: the same in the group copy and the peers' copies
            sns = [struct.unpack('>I', g['ext'][0][1][2:6])[0] for g in allp]
            assert sns == [n] * 3, 'sequence numbers %s, expected %d in every copy' % (sns, n)
        # FSSM alone: the group, no peer
        assert modify(s, update_far(apply=APPLY_FSSM))
        got, grp = sent_to(r, *peers)
        assert not any(got) and len(grp) == 1, 'FSSM alone: peers %s group %d' % ([len(x) for x in got], len(grp))
    finally:
        for p in peers:
            p.close()
        U.delete(s)


@test
def t_U12_5_unicast_only_session():
    # TS 29.244 cl. 5.34.2.2: no multicast transport over N3mb: no PLLSSM, a FAR that drops until the F-TEIDs are known
    r, s = drop_session(0xe321, flags=0x00, params=False)
    peer = GtpuPeer('127.0.0.41')
    try:
        assert find(r, T_N4MB_INFO) is None, 'a low layer SSM was returned'
        assert not sent_to(r, peer)[0][0], 'DROP forwarded'
        m = modify(s, update_far(apply=APPLY_MBSU, extra=(add_unicast(1, 0x4444, '127.0.0.41'),)))
        assert m and cause(m) == CAUSE_ACCEPTED, '%s/%s' % (cause(m) if m else None, offending(m) if m else None)
        got = sent_to(r, peer)[0][0]
        assert len(got) == 1 and gtpu_parse(got[0])['teid'] == 0x4444, 'unicast-only session: %d packets' % len(got)
    finally:
        peer.close()
        U.delete(s)
    # MBSU and its peers in the Create FAR (the restoration of TS 23.527, TS 29.244 cl. 5.34.2.2 NOTE)
    cfar = ie(T_CREATE_FAR, [ie(T_FAR_ID, struct.pack('>I', 1)), ie(T_APPLY, struct.pack('>H', APPLY_MBSU)),
                             add_unicast(1, 0x5555, '127.0.0.41'), add_unicast(2, 0x6666, '127.0.0.42')])
    r = U.establish(replace_ie(mc_request(0xe322, b'\x02' + V4G, None, flags=0x00), T_CREATE_FAR, cfar))
    s = accepted(r, 'MBSU with Add MBS Unicast Parameters in the Create FAR')
    peers = [GtpuPeer('127.0.0.41'), GtpuPeer('127.0.0.42')]
    try:
        got = sent_to(r, *peers)[0]
        assert [len(x) for x in got] == [1, 1], 'copies %s' % [len(x) for x in got]
        assert [gtpu_parse(x[0])['teid'] for x in got] == [0x5555, 0x6666]
    finally:
        for p in peers:
            p.close()
        U.delete(s)


@test
def t_U08_mbsu_requires_a_peer():
    # Create FAR: MBSU without Add MBS Unicast Parameters
    cfar = ie(T_CREATE_FAR, [ie(T_FAR_ID, struct.pack('>I', 1)), ie(T_APPLY, struct.pack('>H', APPLY_MBSU))])
    r = U.establish(replace_ie(mc_request(0xe331, b'\x02' + V4G, None), T_CREATE_FAR, cfar))
    assert r and cause(r) == CAUSE_CONDITIONAL_MISSING and offending(r) == T_ADD_UNICAST, '%s/%s' % (
        cause(r) if r else None, offending(r) if r else None)
    # Update FAR: MBSU with no peer provisioned
    r, s = drop_session(0xe332)
    try:
        m = modify(s, update_far(apply=APPLY_MBSU))
        assert m and cause(m) == CAUSE_CONDITIONAL_MISSING and offending(m) == T_ADD_UNICAST, '%s/%s' % (
            cause(m) if m else None, offending(m) if m else None)
        # the peer added by the same request is not kept when the request fails
        m = modify(s, update_far(apply=APPLY_MBSU | APPLY_FSSM, extra=(add_unicast(1),)), update_far(far_id=99, apply=APPLY_DROP))
        assert m and cause(m) == CAUSE_MANDATORY_INCORRECT
        m = modify(s, update_far(apply=APPLY_MBSU))
        assert m and cause(m) == CAUSE_CONDITIONAL_MISSING, 'the peer of the failed request was kept'
    finally:
        U.delete(s)


@test
def t_U08_8_transport_level_marking_of_a_peer():
    r, s = drop_session(0xe341)
    peer = GtpuPeer('127.0.0.41')
    try:
        m = modify(s, update_far(apply=APPLY_MBSU, extra=(add_unicast(1, 0x7777, '127.0.0.41', extra=(ie(T_TLM, b'\xb8\xfc'),)),)))
        assert m and cause(m) == CAUSE_ACCEPTED
        got = sent_to(r, peer)[0][0]
        assert len(got) == 1, 'nothing at the peer'
        assert peer.tos and peer.tos[0] >> 2 == 0x2e, 'DSCP of the peer %s' % [hex(t) for t in peer.tos]
    finally:
        peer.close()
        U.delete(s)


def smreq_flags(v):
    return ie(49, bytes([v]))


@test
def t_U08_7_deteid_deletes_all_provisioned_f_teids():
    r, s = drop_session(0xe401)
    peers = [GtpuPeer('127.0.0.41'), GtpuPeer('127.0.0.42')]
    try:
        m = modify(s, update_far(apply=APPLY_MBSU, extra=(add_unicast(1, 0x1111, '127.0.0.41'), add_unicast(2, 0x2222, '127.0.0.42'))))
        assert m and cause(m) == CAUSE_ACCEPTED
        assert [len(x) for x in sent_to(r, *peers)[0]] == [1, 1]
        # TS 29.244 cl. 5.34.2.4: Apply Action from MBSU to BUFF or DROP, with DETEID
        m = modify(s, update_far(apply=APPLY_BUFF), smreq_flags(0x20))
        assert m and cause(m) == CAUSE_ACCEPTED, '%s/%s' % (cause(m) if m else None, offending(m) if m else None)
        # the IDs are free again
        m = modify(s, update_far(extra=(add_unicast(1, 0x1111, '127.0.0.41'), add_unicast(2, 0x2222, '127.0.0.42'))))
        assert m and cause(m) == CAUSE_ACCEPTED, 'the F-TEIDs were not deleted: %s/%s' % (cause(m) if m else None, offending(m) if m else None)
        # without DETEID the F-TEIDs are kept (cl. 5.34.2.4: "Otherwise, MB-UPF will keep all unicast DL N3mb F-TEIDs")
        m = modify(s, update_far(apply=APPLY_DROP))
        assert m and cause(m) == CAUSE_ACCEPTED
        m = modify(s, update_far(apply=APPLY_MBSU))
        assert m and cause(m) == CAUSE_ACCEPTED, 'peers were not kept'
        assert [len(x) for x in sent_to(r, *peers)[0]] == [1, 1], 'kept peers did not receive'
        # DETEID and the new F-TEIDs in one request: the deletion first, then the additions
        m = modify(s, update_far(apply=APPLY_MBSU, extra=(add_unicast(1, 0x9999, '127.0.0.41'),)), smreq_flags(0x20))
        assert m and cause(m) == CAUSE_ACCEPTED, '%s/%s' % (cause(m) if m else None, offending(m) if m else None)
        got = sent_to(r, *peers)[0]
        assert [len(x) for x in got] == [1, 0] and gtpu_parse(got[0][0])['teid'] == 0x9999, 'after DETEID and Add: %s' % [len(x) for x in got]
        # a failing request keeps what DETEID deleted
        m = modify(s, update_far(extra=(add_unicast(3, 0xaaaa, '127.0.0.43'),)), update_far(far_id=99, apply=APPLY_DROP), smreq_flags(0x20))
        assert m and cause(m) == CAUSE_MANDATORY_INCORRECT
        got = sent_to(r, *peers)[0]
        assert [len(x) for x in got] == [1, 0], 'a rejected request with DETEID deleted the peers'
        # other flags are not supported: rejected, not ignored
        m = modify(s, update_far(apply=APPLY_DROP), smreq_flags(0x04))
        assert m and cause(m) == CAUSE_SERVICE_NOT_SUPPORTED and offending(m) == 49, '%s/%s' % (cause(m) if m else None, offending(m) if m else None)
    finally:
        for p in peers:
            p.close()
        U.delete(s)


@test
def t_upf_exits_cleanly_on_sigterm():
    # the configuration has upf.mbs.udptunnel.address: its host name is freed by the allocator that made it
    U.restart()
    U.stop()
    assert U.proc.returncode == 0, 'upfd exited with status %s on SIGTERM, end of its output:\n%s' % (
        U.proc.returncode, open(U.err, errors='replace').read()[-400:])
    assert 'UPF terminate...done' in open(U.err, errors='replace').read()
    U.start()


@test
def t_memory_of_a_deleted_session_is_released():
    """The allocator (talloc) reports at a clean exit every block still allocated: after sessions created and deleted
    none of the session's may be left."""
    U.restart()
    for n in range(3):
        cp = 0xf100 + n
        accepted_and_deleted(U.establish(good(cp_seid=cp)), 'TMGI session')
        accepted_and_deleted(U.establish(mc_request(cp + 0x10, b'\x02' + V4G, None)), 'session with a PDR addressing')
        ssm = ssm_id(0x04, socket.inet_aton('232.2.2.2'), 0x04, socket.inet_aton('10.46.0.1'))
        accepted_and_deleted(U.establish(good(cp_seid=cp + 0x20)[:-1] + [n4mb_ctrl(ssm)]), 'SSM session')
    U.stop()
    status = U.proc.returncode
    report = open(U.err, errors='replace').read()
    U.start()
    assert status == 0 and 'UPF terminate...done' in report, 'the UPF exited with %s' % status
    # ogs_mem_final(): the report is written only if blocks other than the allocator's own are left
    assert 'full talloc report' not in report, 'blocks still allocated at exit:\n' + report[report.index('full talloc report'):][:1500]


@test
def t_pool_port_is_returned_when_the_bind_fails():
    U.restart('{start: 39210, end: 39210}')
    try:
        # the one pool port is in use by another process: the bind fails after the port was taken from the pool
        occ = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        occ.bind(('127.0.0.31', 39210))
        try:
            r = U.establish(with_te(good(cp_seid=0x5061), lit_choose()))
            assert r and cause(r) == CAUSE_SYSTEM_FAILURE and offending(r) == T_LIT, 'bind failure: cause %s offending %s' % (
                cause(r) if r else None, offending(r) if r else None)
        finally:
            occ.close()
        accepted_and_deleted(U.establish(with_te(good(cp_seid=0x5062), lit_choose())), 'the pool port after the failed bind')
    finally:
        U.restart()


@test
def t_two_sessions_on_one_group_with_different_sources():
    # an SSM channel is (source, group): two sessions may share the group and each gets its own source's packets
    s1, g1 = data_session(0xe501, b'\x02' + V4G, b'\x02' + socket.inet_aton('10.46.0.1'))
    s2, g2 = data_session(0xe502, b'\x02' + V4G, b'\x02' + socket.inet_aton('10.46.0.2'))
    assert g1 != g2, 'the sessions share their N3mb group'
    l1, l2 = N3mbListener(g1), N3mbListener(g2)
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        assert inner_payload_seen(l1.packets()) and not inner_payload_seen(l2.packets()), 'source 1 not delivered to session 1 only'
        send_multicast('10.46.0.2', '232.1.1.1')
        assert inner_payload_seen(l2.packets()) and not inner_payload_seen(l1.packets()), 'source 2 not delivered to session 2 only'
        # the session that owned the group's slot ends: the other one keeps its data
        U.delete(s1)
        send_multicast('10.46.0.2', '232.1.1.1')
        assert inner_payload_seen(l2.packets()), 'session 2 lost its data when session 1 ended'
        send_multicast('10.46.0.1', '232.1.1.1')
        assert not inner_payload_seen(l1.packets()) and not inner_payload_seen(l2.packets()), 'data of the ended session delivered'
        # the session set up while the first one is gone can share again, the older one is the other way round
        s3, g3 = data_session(0xe503, b'\x02' + V4G, b'\x02' + socket.inet_aton('10.46.0.1'))
        l3 = N3mbListener(g3)
        try:
            send_multicast('10.46.0.1', '232.1.1.1')
            assert inner_payload_seen(l3.packets()), 'new session for source 1 got nothing'
            U.delete(s2)
            send_multicast('10.46.0.1', '232.1.1.1')
            assert inner_payload_seen(l3.packets()), 'session 3 lost its data when session 2 ended'
        finally:
            l3.close()
            U.delete(s3)
    finally:
        l1.close()
        l2.close()
        U.delete(s1)
        U.delete(s2)


@test
def t_U04_data_from_an_any_source_join_reaches_n3mb():
    # JMBSSM with no source address: the join is (*,G) and the packets of any source are routed to the UPF
    U.restart('0', ROUTER)
    try:
        subprocess.run(['ip', 'route', 'add', '232.0.0.0/8', 'dev', 'v1'], stderr=subprocess.DEVNULL)
        r = U.establish(mc_request(0xb042, b'\x02' + V4G, None, flags=0x03))
        s = accepted(r, 'any source session')
        lst = N3mbListener(mc_group_of(r))
        try:
            routed_sender()
            assert inner_payload_seen(lst.packets()), 'data of the (*,G) join did not reach N3mb'
            # a source the join never named
            subprocess.run(['ip', 'addr', 'add', '10.99.0.3/24', 'dev', 'v1'], stderr=subprocess.DEVNULL)
            routed_sender(src='10.99.0.3')
            assert inner_payload_seen(lst.packets()), 'data of a second source did not reach N3mb'
        finally:
            lst.close()
        U.delete(s)
    finally:
        U.restart()


def routed_sender6(src='fd99::2', dst='ff3e::1234', payload=b'mbs-data'):
    sk = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
    sk.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_HOPS, 8)
    sk.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_IF, socket.if_nametoindex('v1'))
    sk.bind((src, 0))
    sk.sendto(payload, (dst, 5000))
    sk.close()


@test
def t_U04_4_data_from_ipv6_joins_reaches_n3mb():
    U.restart('0', ROUTER)
    try:
        for cmd in (['ip', 'addr', 'add', 'fd99::1/64', 'dev', 'v0', 'nodad'], ['ip', 'addr', 'add', 'fd99::2/64', 'dev', 'v1', 'nodad'],
                    ['ip', 'route', 'add', 'ff3e::/16', 'dev', 'v1'],
                    ['sysctl', '-q', '-w', 'net.ipv6.conf.all.accept_local=1', 'net.ipv6.conf.v0.accept_local=1',
                     'net.ipv6.conf.all.forwarding=1']):
            subprocess.run(cmd, stderr=subprocess.DEVNULL)
        src6 = socket.inet_pton(socket.AF_INET6, 'fd99::2')
        for name, srcie, cp in (('source specific', b'\x01' + src6, 0xb051), ('any source', None, 0xb052)):
            r = U.establish(mc_request(cp, b'\x01' + G6, srcie, flags=0x03))
            s = accepted(r, 'IPv6 ' + name)
            lst = N3mbListener(mc_group_of(r))
            try:
                routed_sender6()
                assert inner_payload_seen(lst.packets()), 'data of the IPv6 %s join did not reach N3mb' % name
            finally:
                lst.close()
            U.delete(s)
    finally:
        U.restart()


def mti(c_teid, dest, src, dest_tl=0x04, src_tl=0x04):
    """Multicast Transport Information (Figure 8.2.207-1) for the MBS Session N4mb Control Information."""
    return ie(T_MC_TRANSPORT, b'\x00' + struct.pack('>I', c_teid) + bytes([dest_tl]) + dest + bytes([src_tl]) + src)


def resti_request(cp_seid, info, flags=0x04, extra_ctrl=()):
    """PFCP session restoration: the RESTI flag and, instead of PLLSSM, the low layer SSM and the C-TEID of the session
    before the MB-UPF restarted (TS 29.244 cl. 5.34.2.1A NOTE 1, Table 7.5.2.1-4)."""
    ies = mc_request(cp_seid, b'\x02' + V4G, None, flags=None)
    ctrl = n4mb_ctrl(flags=flags, extra=([info] if info is not None else []) + list(extra_ctrl))
    return replace_ie(ies, T_N4MB_CTRL, ctrl)


UPF_ADDR = socket.inet_aton('127.0.0.31')


@test
def t_U02_9_restoration_with_the_requested_llssm_and_c_teid():
    group = socket.inet_aton('239.77.0.1')
    r = U.establish(resti_request(0xe601, mti(0x00abcdef, group, UPF_ADDR)))
    s = accepted(r, 'restoration')
    assert find(r, T_N4MB_INFO) is None, 'a low layer SSM was returned though PLLSSM was not set'
    lst = N3mbListener('239.77.0.1')
    try:
        send_multicast('10.46.0.1', '232.1.1.1')
        pk = lst.packets()
        assert pk, 'nothing on the requested group'
        assert gtpu_parse(pk[0])['teid'] == 0x00abcdef, 'TEID %08x, requested 00abcdef' % gtpu_parse(pk[0])['teid']
        # the group is held: another restoration for it is refused, whatever its C-TEID
        r2 = U.establish(resti_request(0xe602, mti(0x1234, group, UPF_ADDR)))
        assert r2 and cause(r2) == CAUSE_RESTORATION_FAILURE and offending(r2) == T_MC_TRANSPORT, 'same group: %s/%s' % (
            cause(r2) if r2 else None, offending(r2) if r2 else None)
    finally:
        lst.close()
    # and what the allocation gives to a session without RESTI is never a held group: sessions asking for the
    # addresses the allocator would hand out next
    held = [accepted(U.establish(resti_request(0xe610 + i, mti(0x100 + i, socket.inet_aton('239.0.0.%d' % (4 + 3 + i)), UPF_ADDR))), 'held group %d' % i) for i in range(5)]
    got = []
    for i in range(6):
        r = U.establish(good(cp_seid=0xe620 + i))
        held.append(accepted(r, 'allocated session %d' % i))
        got.append(mc_group_of(r))
    taken = ['239.0.0.%d' % (4 + 3 + i) for i in range(5)]
    assert len(set(got)) == len(got) and not set(got) & set(taken), 'allocated groups %s, held by restoration %s' % (got, taken)
    for h in held:
        U.delete(h)
    # after the restoration session ends, the group is free again
    U.delete(s)
    accepted_and_deleted(U.establish(resti_request(0xe603, mti(0x1234, group, UPF_ADDR))), 'the group after the session ended')


@test
def t_U02_9_restoration_that_cannot_be_served():
    ok_group = socket.inet_aton('239.77.0.2')
    # a source that is not an address of the MB-UPF
    r = U.establish(resti_request(0xe631, mti(7, ok_group, socket.inet_aton('10.9.9.9'))))
    assert r and cause(r) == CAUSE_RESTORATION_FAILURE and offending(r) == T_MC_TRANSPORT, 'foreign source: %s/%s' % (cause(r) if r else None, offending(r) if r else None)
    # IPv6 low layer SSM with no IPv6 GTP-U socket on the MB-UPF
    r = U.establish(resti_request(0xe632, mti(7, G6, S6, dest_tl=0x50, src_tl=0x50)))
    assert r and cause(r) == CAUSE_RESTORATION_FAILURE, 'IPv6 without socket: %s' % (cause(r) if r else None)
    # content that is not valid: not a multicast group, truncated, bad address type/length combination
    r = U.establish(resti_request(0xe633, mti(7, socket.inet_aton('10.1.1.1'), UPF_ADDR)))
    assert r and cause(r) == CAUSE_MANDATORY_INCORRECT and offending(r) == T_MC_TRANSPORT, 'unicast group: %s/%s' % (cause(r) if r else None, offending(r) if r else None)
    r = U.establish(resti_request(0xe634, ie(T_MC_TRANSPORT, b'\x00\x00\x00\x00\x07\x04' + ok_group[:2])))
    assert r and cause(r) == CAUSE_INVALID_LENGTH and offending(r) == T_MC_TRANSPORT, 'truncated: %s/%s' % (cause(r) if r else None, offending(r) if r else None)
    r = U.establish(resti_request(0xe635, mti(7, ok_group, UPF_ADDR, dest_tl=0x10)))
    assert r and cause(r) == CAUSE_MANDATORY_INCORRECT, 'type 0 length 16: %s' % (cause(r) if r else None)
    r = U.establish(resti_request(0xe636, mti(7, ok_group, UPF_ADDR) + b'\x00'))
    assert r is None or cause(r) != CAUSE_ACCEPTED, 'trailing octet accepted'
    # the Local Ingress Tunnel the MB-SMF chose is not available (port held by another process): restoration failure
    occ = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    occ.bind(('127.0.0.31', 39300))
    try:
        lit = ie(T_LIT, b'\x01' + struct.pack('>H', 39300) + socket.inet_aton('127.0.0.31'))
        r = U.establish(resti_request(0xe637, mti(7, ok_group, UPF_ADDR)) + [ie(T_CREATE_TE, [lit])])
        assert r and cause(r) == CAUSE_RESTORATION_FAILURE and offending(r) == T_LIT, 'ingress tunnel in use: %s/%s' % (cause(r) if r else None, offending(r) if r else None)
        # without RESTI the same bind failure is a system failure as before
        r = U.establish(with_te(good(cp_seid=0xe638), lit))
        assert r and cause(r) == CAUSE_SYSTEM_FAILURE, 'no RESTI: %s' % (cause(r) if r else None)
    finally:
        occ.close()
    # the failures left nothing: the group is free
    accepted_and_deleted(U.establish(resti_request(0xe639, mti(7, ok_group, UPF_ADDR))), 'after the failures')
    # the Multicast Transport Information without RESTI is not a request for those addresses
    r = U.establish(replace_ie(good(cp_seid=0xe63a), T_N4MB_CTRL, n4mb_ctrl(flags=0x01, extra=[mti(9, ok_group, UPF_ADDR)])))
    s = accepted(r, 'MTI without RESTI')
    assert mc_group_of(r) != '239.77.0.2', 'the group of an IE sent without RESTI was used'
    U.delete(s)


T_REMOVE_PDR, T_REMOVE_FAR, T_REMOVE_QER = 15, 16, 18


def smf_session(cp_seid):
    """The establishment the MB-SMF sends for a session with an ingress tunnel (smf_n4mb_build_session_establishment_request):
    one flow, its PDR naming Traffic Endpoint 1, the Create Traffic Endpoint with the Local Ingress Tunnel, PLLSSM."""
    pdr1 = pdr(pdi_extra=(ie(T_TE_ID, b'\x01'),))
    return [node_id(), fseid(cp_seid), pdr1, far(), qer(), ie(T_CREATE_TE, [ie(T_TE_ID, b'\x01'), lit_choose()]),
            n4mb_ctrl(flags=0x01)]


def flow_pdr(n, precedence=10, qfi=None, te=b'\x01', drop=()):
    """Create PDR of a flow added by an MBS session update (smf_n4mb_build_session_modification_request): the PDI names
    the session's Traffic Endpoint."""
    pdi = [ie(T_SRC_IF, b'\x01')] + ([ie(T_TE_ID, te)] if te else []) + ([ie(T_QFI, bytes([qfi]))] if qfi else [])
    parts = [ie(T_PDR_ID, struct.pack('>H', n)), ie(T_PRECEDENCE, struct.pack('>I', precedence)), ie(T_PDI, pdi),
             ie(T_FAR_ID, struct.pack('>I', n)), ie(T_QER_ID, struct.pack('>I', n))]
    return ie(T_CREATE_PDR, [x for x in parts if struct.unpack('>H', x[:2])[0] not in drop])


def flow_rules(n, qfi, precedence=10):
    """The three Create IEs of the flow: PDR, FAR (FSSM with the MBS Multicast Parameters) and QER (IQFISN)."""
    return [flow_pdr(n, precedence, qfi), far(far_id=n), qer(qer_id=n, qfi=qfi, indications=0x01)]


def removal(n):
    return [ie(T_REMOVE_PDR, [ie(T_PDR_ID, struct.pack('>H', n))]), ie(T_REMOVE_FAR, [ie(T_FAR_ID, struct.pack('>I', n))]),
            ie(T_REMOVE_QER, [ie(T_QER_ID, struct.pack('>I', n))])]


def ip_packet(dst='232.1.1.1', src='10.46.0.1', dport=5000, payload=b'mbs-data'):
    udp = struct.pack('>HHHH', 4000, dport, 8 + len(payload), 0) + payload
    hdr = struct.pack('>BBHHHBBH4s4s', 0x45, 0, 20 + len(udp), 0, 0, 8, 17, 0, socket.inet_aton(src), socket.inet_aton(dst))
    csum = sum(struct.unpack('>10H', hdr))
    csum = (csum & 0xffff) + (csum >> 16)
    csum = (csum & 0xffff) + (csum >> 16)
    return hdr[:10] + struct.pack('>H', ~csum & 0xffff) + hdr[12:] + udp


def qfi_of_tunnel_packet(r):
    """Sends a packet into the session's ingress tunnel and returns the QFI of the G-PDU on the N3mb group."""
    lit = created_te(r)[T_LIT]
    port, addr = struct.unpack('>H', lit[1:3])[0], socket.inet_ntoa(lit[3:7])
    lst = N3mbListener(mc_group_of(r))
    try:
        sk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sk.sendto(ip_packet(), (addr, port))
        sk.close()
        pk = lst.packets()
    finally:
        lst.close()
    if not pk:
        return None
    g = gtpu_parse(pk[0])
    return g['ext'][0][1][1] & 0x3f if g['ext'] else None


@test
def t_flow_added_and_removed_by_a_modification():
    r = U.establish(smf_session(0xe701))
    s = accepted(r, 'session of the MB-SMF')
    try:
        assert qfi_of_tunnel_packet(r) == 1, 'the first flow does not carry QFI 1'
        # a flow came: Create PDR, FAR and QER as the MB-SMF builds them (Table 7.5.4.1-1: Create PDR/FAR/QER, N4mb "X")
        m = modify(s, *flow_rules(2, 2))
        assert m and cause(m) == CAUSE_ACCEPTED, 'flow added: cause %s offending %s' % (cause(m) if m else None, offending(m) if m else None)
        assert qfi_of_tunnel_packet(r) == 2, 'the new flow (precedence 10) is not used by the data path'
        # the flow went: Remove PDR, FAR and QER
        m = modify(s, *removal(2))
        assert m and cause(m) == CAUSE_ACCEPTED, 'flow removed: cause %s offending %s' % (cause(m) if m else None, offending(m) if m else None)
        assert qfi_of_tunnel_packet(r) == 1, 'the removed flow is still used'
        # its identifiers are free again
        m = modify(s, *flow_rules(2, 3))
        assert m and cause(m) == CAUSE_ACCEPTED
        assert qfi_of_tunnel_packet(r) == 3
        # a flow added and another removed in one request
        m = modify(s, *(flow_rules(3, 4, precedence=5) + removal(2)))
        assert m and cause(m) == CAUSE_ACCEPTED, '%s/%s' % (cause(m) if m else None, offending(m) if m else None)
        assert qfi_of_tunnel_packet(r) == 4
        m = modify(s, *(removal(3)))
        assert m and cause(m) == CAUSE_ACCEPTED
    finally:
        U.delete(s)


@test
def t_flow_changes_that_are_not_valid_change_nothing():
    r = U.establish(smf_session(0xe711))
    s = accepted(r, 'session of the MB-SMF')
    try:
        def bad(what, cause_want, off_want, *ies):
            m = modify(s, *ies)
            assert m and cause(m) == cause_want and offending(m) == off_want, '%s: cause %s offending %s, wanted %s/%s' % (
                what, cause(m) if m else None, offending(m) if m else None, cause_want, off_want)
            assert qfi_of_tunnel_packet(r) == 1, what + ': the request changed the session'

        bad('PDR ID in use', CAUSE_RULE_FAILURE, T_PDR_ID, *flow_rules(1, 2))
        bad('FAR ID in use', CAUSE_RULE_FAILURE, T_FAR_ID, flow_pdr(2, qfi=2), far(far_id=2), far(far_id=1), qer(qer_id=2))
        bad('QER ID in use', CAUSE_RULE_FAILURE, T_QER_ID, flow_pdr(2, qfi=2), far(far_id=2), qer(qer_id=2), qer(qer_id=1))
        bad('no Precedence', CAUSE_MANDATORY_MISSING, T_PRECEDENCE,
            flow_pdr(2, qfi=2, drop=(T_PRECEDENCE,)), far(far_id=2), qer(qer_id=2))
        bad('PDR without FAR ID', CAUSE_CONDITIONAL_MISSING, T_FAR_ID,
            flow_pdr(2, qfi=2, drop=(T_FAR_ID,)), qer(qer_id=2))
        bad('FAR that nobody creates', CAUSE_MANDATORY_INCORRECT, T_FAR_ID, flow_pdr(2, qfi=2), qer(qer_id=2))
        bad('QER that nobody creates', CAUSE_MANDATORY_INCORRECT, T_QER_ID, flow_pdr(2, qfi=2), far(far_id=2))
        bad('Traffic Endpoint that the session has not', CAUSE_MANDATORY_INCORRECT, 131,
            flow_pdr(2, qfi=2, te=b'\x07'), far(far_id=2), qer(qer_id=2))
        bad('Local Ingress Tunnel in the PDI', CAUSE_SERVICE_NOT_SUPPORTED, T_LIT,
            ie(T_CREATE_PDR, [ie(T_PDR_ID, struct.pack('>H', 2)), ie(T_PRECEDENCE, struct.pack('>I', 10)),
                              ie(T_PDI, [ie(T_SRC_IF, b'\x01'), lit_choose()]), ie(T_FAR_ID, struct.pack('>I', 2)),
                              ie(T_QER_ID, struct.pack('>I', 2))]), far(far_id=2), qer(qer_id=2))
        bad('FSSM without MBS Multicast Parameters', CAUSE_CONDITIONAL_MISSING, T_MBS_MC_PARAMS,
            flow_pdr(2, qfi=2), far(far_id=2, params=False), qer(qer_id=2))
        bad('MBSU without Add MBS Unicast Parameters', CAUSE_CONDITIONAL_MISSING, T_ADD_UNICAST,
            flow_pdr(2, qfi=2), far(far_id=2, apply=APPLY_MBSU, params=False), qer(qer_id=2))
        bad('QER without Gate Status', CAUSE_MANDATORY_MISSING, T_GATE,
            flow_pdr(2, qfi=2), far(far_id=2), qer(qer_id=2, drop=(T_GATE,)))
        bad('FAR without Apply Action', CAUSE_MANDATORY_MISSING, T_APPLY,
            flow_pdr(2, qfi=2), far(far_id=2, drop=(T_APPLY,)), qer(qer_id=2))
        bad('same PDR twice', CAUSE_RULE_FAILURE, T_PDR_ID, flow_pdr(2, qfi=2), flow_pdr(2, qfi=2), far(far_id=2), qer(qer_id=2))
        # a request with a good flow and a bad removal creates nothing
        bad('unknown PDR to remove', CAUSE_MANDATORY_INCORRECT, T_PDR_ID,
            *(flow_rules(2, 2) + [ie(T_REMOVE_PDR, [ie(T_PDR_ID, struct.pack('>H', 99))])]))
        bad('removal without its ID', CAUSE_MANDATORY_MISSING, T_PDR_ID, *(flow_rules(2, 2) + [ie(T_REMOVE_PDR, [ie(T_FAR_ID, struct.pack('>I', 1))])]))
        bad('unknown FAR to remove', CAUSE_MANDATORY_INCORRECT, T_FAR_ID, ie(T_REMOVE_FAR, [ie(T_FAR_ID, struct.pack('>I', 99))]))
        bad('unknown QER to remove', CAUSE_MANDATORY_INCORRECT, T_QER_ID, ie(T_REMOVE_QER, [ie(T_QER_ID, struct.pack('>I', 99))]))
        bad('PDR removed twice', CAUSE_MANDATORY_INCORRECT, T_PDR_ID, *(removal(1)[:1] * 2))
        # the FAR and the QER of a PDR that stays cannot go
        bad('FAR of a PDR that stays', CAUSE_RULE_FAILURE, T_FAR_ID, removal(1)[1])
        bad('QER of a PDR that stays', CAUSE_RULE_FAILURE, T_QER_ID, removal(1)[2])
        # the flow of a request whose Update FAR fails afterwards (the checks of the Apply Action come last) is taken out
        bad('flow and a failing Update FAR', CAUSE_CONDITIONAL_MISSING, T_ADD_UNICAST,
            *(flow_rules(2, 2) + [update_far(apply=APPLY_MBSU)]))
        # none of the failed requests left a rule behind: the identifiers are free
        m = modify(s, *flow_rules(2, 2))
        assert m and cause(m) == CAUSE_ACCEPTED, 'a failed request left rules: %s/%s' % (cause(m) if m else None, offending(m) if m else None)
        assert qfi_of_tunnel_packet(r) == 2
        # a flow whose PDR, FAR and QER all go with the request, in any order, while another is created
        m = modify(s, *(removal(2) + flow_rules(3, 3)))
        assert m and cause(m) == CAUSE_ACCEPTED
        assert qfi_of_tunnel_packet(r) == 3
    finally:
        U.delete(s)


@test
def t_flow_added_to_a_multicast_session_follows_the_session_group():
    r = U.establish(mc_request(0xe721, b'\x02' + V4G, None))
    s = accepted(r, 'session with an N6mb group')
    lst = N3mbListener(mc_group_of(r))
    info = mc_info(b'\x02' + V4G, None)
    try:
        def flow(n, group_info):
            pdi = [ie(T_SRC_IF, b'\x01'), ie(T_QFI, bytes([n]))] + ([group_info] if group_info else [])
            return ie(T_CREATE_PDR, [ie(T_PDR_ID, struct.pack('>H', n)), ie(T_PRECEDENCE, struct.pack('>I', 10)), ie(T_PDI, pdi),
                                     ie(T_FAR_ID, struct.pack('>I', n)), ie(T_QER_ID, struct.pack('>I', n))])

        # the group of the session in the PDI: accepted; another group: one session has one group
        m = modify(s, flow(2, info), far(far_id=2), qer(qer_id=2, qfi=2, indications=1))
        assert m and cause(m) == CAUSE_ACCEPTED, '%s/%s' % (cause(m) if m else None, offending(m) if m else None)
        send_multicast('10.46.0.1', '232.1.1.1')
        pk = lst.packets()
        assert pk and gtpu_parse(pk[0])['ext'][0][1][1] & 0x3f == 2, 'the new flow is not used'
        m = modify(s, flow(3, mc_info(b'\x02' + socket.inet_aton('232.9.9.9'), None)), far(far_id=3), qer(qer_id=3, qfi=3))
        assert m and cause(m) == CAUSE_MANDATORY_INCORRECT and offending(m) == T_IPMC_ADDR, '%s/%s' % (cause(m) if m else None, offending(m) if m else None)
        m = modify(s, *removal(2))
        assert m and cause(m) == CAUSE_ACCEPTED
        send_multicast('10.46.0.1', '232.1.1.1')
        pk = lst.packets()
        assert pk and gtpu_parse(pk[0])['ext'][0][1][1] & 0x3f == 1, 'the removed flow is still used'
    finally:
        lst.close()
        U.delete(s)


@test
def t_flow_with_unicast_peers_added_and_removed():
    r = U.establish(smf_session(0xe731))
    s = accepted(r, 'session of the MB-SMF')
    peer = GtpuPeer('127.0.0.41')
    try:
        # a flow that is forwarded by unicast transport only, its peers in the Create FAR
        cfar = ie(T_CREATE_FAR, [ie(T_FAR_ID, struct.pack('>I', 2)), ie(T_APPLY, struct.pack('>H', APPLY_MBSU)),
                                 add_unicast(1, 0x7171, '127.0.0.41')])
        m = modify(s, flow_pdr(2, 10, 2), cfar, qer(qer_id=2, qfi=2, indications=0x01))
        assert m and cause(m) == CAUSE_ACCEPTED, '%s/%s' % (cause(m) if m else None, offending(m) if m else None)
        lit = created_te(r)[T_LIT]
        sk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sk.sendto(ip_packet(), (socket.inet_ntoa(lit[3:7]), struct.unpack('>H', lit[1:3])[0]))
        sk.close()
        pk = peer.packets()
        assert len(pk) == 1 and gtpu_parse(pk[0])['teid'] == 0x7171, 'the flow with a peer: %d packets' % len(pk)
        assert gtpu_parse(pk[0])['ext'][0][1][1] & 0x3f == 2, 'QFI of the new flow'
        m = modify(s, *removal(2))
        assert m and cause(m) == CAUSE_ACCEPTED
        # the FAR went with its peers: the ID can be used again, with the same peer ID
        m = modify(s, flow_pdr(2, 10, 2), cfar, qer(qer_id=2, qfi=2))
        assert m and cause(m) == CAUSE_ACCEPTED, '%s/%s' % (cause(m) if m else None, offending(m) if m else None)
    finally:
        peer.close()
        U.delete(s)


@test
def t_empty_grouped_ie_is_a_message_with_its_members_missing():
    """TS 29.244 cl. 8.1.1: Length "n" is the length of the content, so a grouped IE of length 0 is a well formed IE
    with no member; cl. 7.6.6 then has the request answered with the missing member, not dropped."""
    def empty(t):
        return ie(t, b'')

    def est(what, t, want_cause, want_off):
        r = U.establish(replace_ie(good(0xf001), t, empty(t)))
        assert r is not None, what + ': no response (the request was dropped)'
        # the clause names "the type of the missing mandatory IE": any of the members that are missing
        assert cause(r) == want_cause and offending(r) in want_off, '%s: cause %s offending %s' % (what, cause(r), offending(r))
        assert U.alive()

    est('empty Create PDR', T_CREATE_PDR, CAUSE_MANDATORY_MISSING, (T_PDR_ID, T_PRECEDENCE, T_PDI))
    est('empty Create FAR', T_CREATE_FAR, CAUSE_MANDATORY_MISSING, (T_FAR_ID, T_APPLY))
    est('empty Create QER', T_CREATE_QER, CAUSE_MANDATORY_MISSING, (T_QER_ID, T_GATE))
    # a grouped IE nested in another
    ies = replace_ie(good(0xf002), T_CREATE_PDR, ie(T_CREATE_PDR, [ie(T_PDR_ID, struct.pack('>H', 1)), ie(T_PRECEDENCE, struct.pack('>I', 1)),
                                                                   ie(T_PDI, b''), ie(T_FAR_ID, struct.pack('>I', 1))]))
    r = U.establish(ies)
    assert r is not None, 'empty PDI: no response'
    assert cause(r) == CAUSE_MANDATORY_MISSING and offending(r) == T_SRC_IF, 'empty PDI: %s/%s' % (cause(r), offending(r))
    # a session that exists: modification and removal requests with empty grouped IEs
    r = U.establish(smf_session(0xf003))
    s = accepted(r, 'session')
    try:
        for what, t, c, o in (('Update FAR', T_UPDATE_FAR, CAUSE_MANDATORY_MISSING, T_FAR_ID),
                              ('Remove PDR', T_REMOVE_PDR, CAUSE_MANDATORY_MISSING, T_PDR_ID),
                              ('Remove FAR', T_REMOVE_FAR, CAUSE_MANDATORY_MISSING, T_FAR_ID),
                              ('Remove QER', T_REMOVE_QER, CAUSE_MANDATORY_MISSING, T_QER_ID)):
            m = modify(s, empty(t))
            assert m is not None, 'empty %s: no response' % what
            assert cause(m) == c and offending(m) == o, 'empty %s: cause %s offending %s' % (what, cause(m), offending(m))
        m = modify(s, ie(T_CREATE_PDR, b''))
        assert m is not None and cause(m) == CAUSE_MANDATORY_MISSING and offending(m) == T_PDR_ID, 'empty Create PDR in a modification'
        assert U.alive()
    finally:
        U.delete(s)
    # a node related message: an empty optional grouped IE (UE IP Address Pool Information, 233) does not stop the association
    r = U.request(5, [ie(T_NODE_ID, b'\x00' + socket.inet_aton('127.0.0.77')), ie(96, struct.pack('>I', 3900000000)), empty(233)])
    assert r is not None and r['type'] == 6, 'Association Setup Request with an empty grouped IE: no response'


# E-TESTS-BELOW


def main():
    global U
    U = Upf()
    failed = 0
    ran = 0
    try:
        for f in TESTS:
            if FILTER and not any(x in f.__name__ for x in FILTER):
                continue
            ran += 1
            try:
                f()
                if not U.alive():
                    raise AssertionError('upfd died')
                print('PASS', f.__name__)
            except Exception as e:
                failed += 1
                print('FAIL', f.__name__, '-', repr(e))
                if not U.alive():
                    print('upfd exited with status %s; end of its log:' % U.proc.returncode)
                    print('\n'.join(U.log_text().splitlines()[-12:]))
                    U.start()
    finally:
        U.stop()
    print('%d run, %d failed' % (ran, failed))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
