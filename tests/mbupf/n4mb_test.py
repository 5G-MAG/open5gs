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
T_CAUSE, T_SRC_IF, T_NET_INST, T_GATE, T_PRECEDENCE, T_OFFENDING, T_DST_IF, T_APPLY = 19, 20, 22, 25, 29, 40, 42, 44
T_PDR_ID, T_F_SEID, T_NODE_ID, T_OHC, T_FAR_ID, T_QER_ID, T_QFI = 56, 57, 60, 84, 108, 109, 124
T_CREATE_TE, T_CREATED_TE, T_TE_ID = 127, 128, 131
T_IPMC_INFO, T_IPMC_ADDR, T_SRC_IP = 188, 191, 192
T_PARTIAL = 272
T_N4MB_CTRL, T_MBS_MC_PARAMS, T_N4MB_INFO, T_MBS_ID, T_MC_TRANSPORT, T_N4MB_FLAGS, T_LIT = 300, 301, 303, 305, 306, 307, 308

CAUSE_ACCEPTED, CAUSE_PARTIAL, CAUSE_REJECTED, CAUSE_CONTEXT_NOT_FOUND = 1, 3, 64, 65
CAUSE_MANDATORY_MISSING, CAUSE_CONDITIONAL_MISSING, CAUSE_INVALID_LENGTH, CAUSE_MANDATORY_INCORRECT = 66, 67, 68, 69
CAUSE_RULE_FAILURE, CAUSE_NO_RESOURCES, CAUSE_SYSTEM_FAILURE = 73, 75, 77



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
        self.start()

    def write_config(self, port_yaml, extra_upf=''):
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
  session:
    - subnet: 10.46.0.0/16
      gateway: 10.46.0.1
      dev: ogstunt
  mbs:
    udptunnel:
      address: 127.0.0.31
      port: %s
%s""" % (self.log, port_yaml, extra_upf))

    def restart(self, port_yaml='0', extra_upf=''):
        self.stop()
        self.write_config(port_yaml, extra_upf)
        self.start()

    def start(self):
        self.proc = subprocess.Popen([UPFD, '-c', self.cfg], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1.5)
        assert self.proc.poll() is None, 'upfd did not start, see ' + self.log
        if self.sock:
            self.sock.close()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(CP)
        self.sock.settimeout(2.0)
        self.associate()
        self.setup_net()

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


def far(far_id=1, apply=0x0008, params=True, drop=()):
    parts = [ie(T_FAR_ID, struct.pack('>I', far_id)), ie(T_APPLY, struct.pack('>H', apply))]
    if params:
        parts.append(ie(T_MBS_MC_PARAMS, [ie(T_DST_IF, b'\x00'), ie(T_OHC, b'\x00\x04')]))
    parts = [p for p in parts if struct.unpack('>H', p[:2])[0] not in drop]
    return ie(T_CREATE_FAR, parts)


def qer(qer_id=1):
    return ie(T_CREATE_QER, [ie(T_QER_ID, struct.pack('>I', qer_id)), ie(T_GATE, b'\x00'), ie(T_QFI, b'\x01')])


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
    accepted_and_deleted(U.establish(replace_ie(good(cp_seid=0x6002, flags=0x00), T_CREATE_FAR, far(apply=0x0002, params=False))), 'DROP')
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
        self.sock.settimeout(0.7)

    def packets(self):
        out = []
        try:
            while True:
                out.append(self.sock.recv(65535))
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
            except AssertionError as e:
                failed += 1
                print('FAIL', f.__name__, '-', e)
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
