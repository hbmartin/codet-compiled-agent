"""Credential-free, source-bound reading of public GitHub Actions runs."""
import hashlib
import json
import re
import time
import urllib.error
import urllib.request


def public_request(url):
    request = urllib.request.Request(url, headers={
        'Accept': 'application/vnd.github+json', 'User-Agent': 'codet-evidence-audit',
        'X-GitHub-Api-Version': '2022-11-28'})
    with urllib.request.urlopen(request, timeout=30) as response:
        data = response.read(8 * 1024 * 1024 + 1)
        if len(data) > 8 * 1024 * 1024:
            raise ValueError('Public CI response exceeds bound')
        return json.loads(data), dict(url=url, status=response.status,
            sha256=hashlib.sha256(data).hexdigest(), response=json.loads(data),
            headers={key: response.headers[key] for key in ('ETag', 'X-GitHub-Request-Id', 'X-RateLimit-Remaining') if key in response.headers})


def read_public_ci(repository, run_id, head, *, request=public_request, pause=time.sleep, clock=time.monotonic):
    if re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository) is None or not str(run_id).isdigit():
        raise ValueError('Invalid public CI repository/run identity')
    base = 'https://api.github.com/repos/' + repository + '/actions/runs/' + str(run_id)
    provenance = []
    attempt = None
    deadline = clock() + 1700
    def get(url):
        try:
            value, observation = request(url)
        except (OSError, urllib.error.URLError, TimeoutError) as error:
            raise ValueError('Public CI request unavailable: ' + url + ': ' + str(error)) from error
        provenance.append(observation)
        return value
    def validate(run):
        nonlocal attempt
        if (run.get('id') != int(run_id) or run.get('repository', {}).get('full_name') != repository or
                run.get('head_sha') != head or type(run.get('run_attempt')) is not int or run['run_attempt'] <= 0):
            raise ValueError('Public CI identity/head mismatch')
        if attempt is not None and attempt != run['run_attempt']:
            raise ValueError('Public CI attempt changed')
        attempt = run['run_attempt']
    while True:
        run = get(base)
        validate(run)
        if run.get('status') == 'completed':
            break
        if clock() >= deadline:
            raise TimeoutError('Public CI did not complete')
        pause(min(60, max(0, deadline - clock())))
    jobs, total, page = [], None, 1
    while True:
        value = get(base + '/attempts/' + str(attempt) + '/jobs?per_page=100&page=' + str(page))
        if type(value.get('total_count')) is not int or not isinstance(value.get('jobs'), list):
            raise ValueError('Malformed public CI job page')
        if total is not None and total != value['total_count']:
            raise ValueError('Public CI job inventory changed')
        total = value['total_count']
        batch = value['jobs']
        if len(batch) > 100 or (not batch and len(jobs) != total):
            raise ValueError('Incomplete public CI job inventory')
        jobs.extend(batch)
        if len(jobs) >= total:
            break
        page += 1
        if page > 1000:
            raise ValueError('Public CI job pagination exceeds bound')
    final = get(base)
    validate(final)
    if (final.get('status') != 'completed' or final.get('conclusion') != 'success' or
            run.get('conclusion') != 'success' or not jobs or len(jobs) != total or
            len({job.get('id') for job in jobs}) != len(jobs) or
            any(job.get('run_id') != int(run_id) or job.get('head_sha') != head or
                job.get('status') != 'completed' or job.get('conclusion') != 'success' for job in jobs)):
        raise ValueError('Required public CI did not succeed with complete job coverage')
    return dict(repository=repository, run_id=int(run_id), head_sha=head, attempt=attempt,
                run=final, jobs=jobs, requests=provenance)
