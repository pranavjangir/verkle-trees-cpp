// import level from 'level' 
import { SecureTrie as Trie } from 'merkle-patricia-tree'

// const db = new level('./testdb')
// const trie = new Trie(db)
const trie = new Trie()

// async function test() {
//   await trie.put(Buffer.from('test'), Buffer.from('one'))
//   const value = await trie.get(Buffer.from('test'))
//   console.log(value.toString()) // 'one'
// }

// test()

async function test2() {
  await trie.put(Buffer.from('test'), Buffer.from('one'))
  await trie.put(Buffer.from('test1'), Buffer.from('two'))
  await trie.put(Buffer.from('test2'), Buffer.from('three'))
  await trie.put(Buffer.from('test3'), Buffer.from('four'))
  await trie.put(Buffer.from('test4'), Buffer.from('four'))
  await trie.put(Buffer.from('test5'), Buffer.from('four'))
  await trie.put(Buffer.from('test6'), Buffer.from('four'))
  await trie.put(Buffer.from('test7'), Buffer.from('four'))
  await trie.put(Buffer.from('test8'), Buffer.from('four'))
  await trie.put(Buffer.from('test9'), Buffer.from('four'))
  const proof = await Trie.createProof(trie, Buffer.from('test'))
  const value = await Trie.verifyProof(trie.root, Buffer.from('test'), proof)
  console.log("proof is ", proof)
  console.log(value.toString()) // 'one'
  // console.log(JSON.stringify(trie))
  const roughObjSize = JSON.stringify(trie).length;
  console.log(roughObjSize)
}

test2()