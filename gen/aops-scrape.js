const https = require('https');
const fs = require('fs');
const path = require('path');
const readline = require("readline");
const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

function clean_possible(possible) {
    rep_basic = { "$": "", "\\begin{algin*}": "", "\end{algin*}": "", "\\[": "", "\\]": "", "\\(": "", "\\)": "",
            "\\leq": "=", "\le": "=", "\geq": "=", "\ge": "=", 
            "\neq": "=", "\ne": "=", "<": "=", ">": "=", " ": "", ",": "", ".": "",
            "p": "f", "g": "f", "h": "f",
            "[": "(", "]": "(",
            "{": "", "}": "",
            "&=": "=", "|": "=", "\\\\": "" }
    avoid = ["\\"]
    req = ["f"]
    rep_var = [["x", "a", "n", "x1_1"], ["y", "b", "m", "x1_2"], ["z", "c", "x1_3"], ["w"]]

    possible = possible.map(s => s.toLowerCase())
    for (const [k, v] of Object.entries(rep_basic)) {
        possible = possible.map(s => s.replaceAll(k, v))
    }
    // filter out function domain/codomain def
    possible = possible.filter(s => s != "f")
    for (const a of avoid) {
        possible = possible.filter(s => !s.includes(a))
    }
    for (const r of req) {
        possible = possible.filter(s => s.includes(r))
    }
    for (let i = 0; i < rep_var.length; i++) {
        for (const v of rep_var[i]) {
            possible = possible.map(s => s.replaceAll(v, "x" + (i + 1)));
        }
    }
    for (let i = 2; i < 4; i++) {
        for (let v = 1; v <= 4; v++) {
            possible = possible.map(s => s.replaceAll("x" + v + "^" + i, ("x" + v).repeat(i)));
        }
    }

    avoid2 = ["^", "l", "t", "g"]
    for (const a of avoid2) {
        possible = possible.filter(s => !s.includes(a))
    }
    
    return possible
}

const basePostData = {
    category_type: "search",
    log_visit: 0,
    required_tag: "",
    fetch_before: 0,
    user_id: 0,
    fetch_archived: 0,
    fetch_announcements: 0,
    search_settings: JSON.stringify({
        //post_text: "\"function\" \"functions\"",
        //post_text: "find all functions",
        //tags: "FE",
        //tags: "functional equation",
        //first_post: 1, 
        forums: [6],
        forums_action: "include",
        //sort_by: "post_time_asc"
        search_text:"\"f(\"",
        //include_users: [29428], // pco
        sort_by: "score"
    }),
    a: "fetch_topics",
    aops_logged_in: false,
    aops_user_id: 1,
    aops_session_id: "21d6f40cfb511982e4424e0e250a9557"
}

function post(idx) {
    //const postData = `category_type=search&log_visit=0&required_tag=&fetch_before=0&user_id=0&fetch_archived=0&fetch_announcements=0&search_settings=%7B%22tags%22%3A%22FE%22%2C%22first_post%22%3A0%2C%22forums%22%3A%5B6%5D%2C%22forums_action%22%3A%22include%22%2C%22sort_by%22%3A%22post_time_asc%22%7D&start_search_id=${15 * idx}&a=fetch_topics&aops_logged_in=false&aops_user_id=1&aops_session_id=21d6f40cfb511982e4424e0e250a9557`
    //const postData = `category_type=search&log_visit=0&required_tag=&fetch_before=0&user_id=0&fetch_archived=0&fetch_announcements=0&search_settings=%7B%22search_text%22%3A%22%5C%22f(x)%5C%22%22%2C%22forums%22%3A%5B6%5D%2C%22forums_action%22%3A%22include%22%2C%22sort_by%22%3A%22post_time_asc%22%7Dforums_action%22%3A%22include%22%2C%22sort_by%22%3A%22post_time_asc%22%7D&start_search_id=${15 * idx}&a=fetch_topics&aops_logged_in=false&aops_user_id=1&aops_session_id=21d6f40cfb511982e4424e0e250a9557`

// 21d6f40cfb511982e4424e0e250a9557
    const postData = {
        ...basePostData,
        start_search_id: 15 * idx,
    }
    const postDataString = new URLSearchParams(Object.entries(postData)).toString();

    const options = {
        hostname: 'artofproblemsolving.com',
        port: 443,
        path: '/m/community/ajax.php',
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
            'Content-Length': postDataString.length,
        }
    };

    return new Promise((resolve, reject) => {
        const req = https.request(options, (res) => {
            console.log(`Query: ${idx}, StatusCode: ${res.statusCode}`);
            //console.log('headers:', res.headers);

            const body = []
            res.on('data', (chunk) => body.push(chunk))
            res.on('end', () => {
                const resString = Buffer.concat(body).toString()
                resolve(resString);
            })
        });

        req.on('error', (e) => {
            reject(e);
        });

        req.on('timeout', () => {
            req.destroy()
            console.log("Request timed out");
            reject('Request timed out')
        })

        req.write(postDataString);
        req.end();
    });
}

const re = /alt=\"([^\"]*)\"/g

function clean_data(data) {
    data = JSON.parse(data).response.topics;
    possible = [].concat.apply([], data.map(t => [...t.preview.matchAll(re)].map(m => m[1])));
    return clean_possible(possible);
}

function get(st, en) {
    return new Promise((res, rej) => {
        let prom = [...Array(en - st).keys()].map(i => i + st).map(i => new Promise((res) => {
            setTimeout(async () => {
                data = await post(i)
                res(clean_data(data));
            }, Math.floor((en - st) * (256 * Math.random() + 64 * Math.random()
                                      + 16 * Math.random() +  4 * Math.random()))
            );
        }));

        Promise.all(prom).then(t => {
            let raw = new Set([].concat.apply([], t));
            res([...raw]);
        }).catch(e => rej(e));
    });
}

rl.question("Start: ", st => {
    st = parseInt(st);
    rl.question("End: ", en => {
        en = parseInt(en);
        get(st, en).then(data => {
            console.log(`Got ${data.length} results`);
            rl.question("Write to file? (y/n): ", ans => {
                if (ans == "y" || ans == "Y") {
                    fs.writeFileSync(path.join(__dirname, 'data',
                        `raw-aops-${basePostData.search_settings.replaceAll(' ', '')}-${st}-${en}.txt`), data.join("\n"))
                    rl.close();
                } else {
                    rl.question("Print? (y/n): ", ans => {
                        if (ans == "y" || ans == "Y") {
                            console.log(data);
                        }
                        rl.close();
                    });
                }
            });
        });
    });
});

