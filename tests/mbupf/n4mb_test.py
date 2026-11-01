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
      port: 0
""" % self.log)
        self.proc = None
        self.sock = None
        self.seq = 100
        self.start()

    def start(self):
        self.proc = subprocess.Popen([UPFD, '-c', self.cfg], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1.5)
        assert self.proc.poll() is None, 'upfd did not start, see ' + self.log
        if self.sock:
            self.sock.close()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(CP)
        self.sock.settimeout(0.6)
        self.associate()

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
                    U.start()
    finally:
        U.stop()
    print('%d run, %d failed' % (ran, failed))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
