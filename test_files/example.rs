//! Showcase of Rust syntax and the nytor Rust lexer.
#![allow(dead_code, unused_imports, unused_variables)]

use std::borrow::Cow;
use std::collections::{BTreeMap, HashMap, HashSet};
use std::fmt::{self, Debug, Display, Formatter};
use std::fs::{self, File, OpenOptions};
use std::io::{self, BufRead, BufReader, Read, Write};
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex, OnceLock};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

const GREETING: &str = "Hello, Rust!\nEscapes: \t tab, \" quote, \\ slash, \u{1F980}";
const RAW_GREETING: &str = r#"A raw string keeps backslashes: \n and quotes: "hello"."#;
const MAX_RETRIES: usize = 3;
static GLOBAL_NAME: OnceLock<String> = OnceLock::new();

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Hash)]
struct Point<T> {
    x: T,
    y: T,
}

impl<T: Display> Display for Point<T> {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        write!(formatter, "({}, {})", self.x, self.y)
    }
}

#[derive(Debug)]
enum AppError {
    Io(io::Error),
    MissingValue(String),
}

impl Display for AppError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Io(error) => write!(formatter, "I/O error: {error}"),
            Self::MissingValue(name) => write!(formatter, "missing value: {name}"),
        }
    }
}

impl std::error::Error for AppError {}

/// Return the longer string. The lifetime connects the inputs to the result.
fn longest<'a>(left: &'a str, right: &'a str) -> &'a str {
    if left.len() >= right.len() { left } else { right }
}

fn parse_count(input: &str) -> Result<usize, std::num::ParseIntError> {
    input.trim().parse::<usize>()
}

fn read_preview(path: &Path) -> io::Result<String> {
    let file = File::open(path)?;
    let mut reader = BufReader::new(file);
    let mut preview = String::new();
    reader.read_to_string(&mut preview)?;
    Ok(preview)
}

fn collection_examples() {
    let mut names: Vec<String> = vec![String::from("Ada"), "Grace".to_owned()];
    names.push(format!("Rust {}", 2026));
    let first = names.first().cloned().unwrap_or_default();
    let lengths: Vec<usize> = names.iter().map(String::len).collect();
    let nonempty: Vec<&String> = names.iter().filter(|name| !name.is_empty()).collect();

    let mut scores: HashMap<String, u32> = HashMap::new();
    scores.insert(first.clone(), 100);
    let score = scores.get(&first).copied().unwrap_or(0);

    let unique: HashSet<&str> = ["red", "green", "red"].into_iter().collect();
    let mut ordered: BTreeMap<&str, usize> = BTreeMap::new();
    ordered.insert("items", names.len());

    println!("lengths={lengths:?}, nonempty={}, score={score}", nonempty.len());
    debug_assert!(!unique.is_empty());
    assert_eq!(ordered.get("items"), Some(&names.len()));
}

fn ownership_and_traits() {
    let borrowed = "borrowed text";
    let owned = borrowed.to_owned();
    let flexible: Cow<'_, str> = Cow::Borrowed(borrowed);
    let shared = Arc::new(Mutex::new(owned));
    let worker_data = Arc::clone(&shared);
    let worker: JoinHandle<usize> = thread::spawn(move || {
        let guard = worker_data.lock().expect("mutex poisoned");
        guard.len()
    });

    let size = worker.join().unwrap_or_default();
    println!("{}: {size}", flexible);
}

fn file_examples(path: &Path) -> io::Result<PathBuf> {
    let mut output = PathBuf::from(path);
    output.set_extension("copy.txt");
    let mut file = OpenOptions::new().write(true).create(true).open(&output)?;
    writeln!(file, "{GREETING}")?;
    file.flush()?;
    let _metadata = fs::metadata(&output)?;
    Ok(output)
}

fn pattern_examples(value: Option<i32>) -> &'static str {
    match value {
        Some(0) => "zero",
        Some(1..=9) => "one digit",
        Some(number) if number < 0 => "negative",
        Some(_) => "many",
        None => "missing",
    }
}

fn main() -> Result<(), AppError> {
    let point = Point { x: 3.14_f64, y: -2.0 };
    let byte = b'R';
    let character = '🦀';
    let escaped_char = '\\';
    let multiline_raw = br##"
        Raw byte strings can span lines.
        Hash markers inside are fine: # and ##.
    "##;

    /* Block comments nest in Rust:
       /* inner comment */
       and the outer comment continues here.
    */
    // Line comments and doc comments are recognized too.

    println!("{point}; {byte}; {character}; {escaped_char}");
    println!(r#"raw format string: {braces} and "quotes""#);
    dbg!(MAX_RETRIES, multiline_raw);

    for number in 0_u32..=10 {
        let square = number * number;
        if square % 2 == 0 && square >= 4 {
            println!("n={number}, square={square}");
        } else if square == 1 {
            continue;
        }
    }

    let answer = parse_count("42").unwrap_or_else(|error| {
        eprintln!("could not parse count: {error}");
        0
    });
    let selected = longest("Rust", "lexer");
    println!("selected={selected}; answer={answer}");

    let optional = Some(7_i32).map(|number| number + 1);
    println!("{}", pattern_examples(optional));
    collection_examples();
    ownership_and_traits();

    let start = Instant::now();
    std::thread::sleep(Duration::from_millis(1));
    println!("elapsed: {:?}", start.elapsed());

    let root = Path::new("test_files");
    let _preview = read_preview(root.join("example.rs").as_path())?;
    let _copy = file_examples(root.join("lexer-output.txt").as_path())?;
    let _ = GLOBAL_NAME.set(String::from("lexer showcase"));

    // Numeric forms, suffixes, ranges, and operators.
    let _numbers = (0, 0xff_u8, 0b1010, 1_000_000usize, 1.25e-3_f64);
    let _flags = true && !false || (answer > 0 && answer <= 100);
    let _closure = |x: i32| -> i32 { x * x };

    Ok(())
}
