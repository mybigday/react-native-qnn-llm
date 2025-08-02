import { useState, useRef, useCallback } from 'react';
import { StyleSheet, View, Text, TextInput, Button } from 'react-native';
import { Context } from 'react-native-qnn-llm';
import * as RNFS from '@dr.pogodin/react-native-fs';
import { pick } from '@react-native-documents/picker';

export default function App() {
  const context = useRef<Context | null>(null);
  const [input, setInput] = useState('');
  const [result, setResult] = useState('');

  const loadContext = useCallback(async () => {
    const [file] = await pick();
    if (!file) {
      return;
    }
    const bundle_path = file.uri;
    const unpack_dir = RNFS.DocumentDirectoryPath + '/unpack';
    context.current = await Context.load({
      bundle_path,
      unpack_dir,
    });
  }, []);

  const process = useCallback(async () => {
    await context.current?.process(input);
  }, [input]);

  const query = useCallback(async () => {
    await context.current?.query(input, (response) => {
      setResult(response);
    });
  }, [input]);

  return (
    <View style={styles.container}>
      <TextInput
        style={styles.input}
        placeholder="Enter your prompt"
        value={input}
        onChangeText={setInput}
      />
      <Button title="Load Model" onPress={loadContext} />
      <Button title="Process" onPress={process} />
      <Button title="Query" onPress={query} />
      <Text>Result: {result}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
  },
  box: {
    width: 60,
    height: 60,
    marginVertical: 20,
  },
  input: {
    width: '80%',
    height: 40,
    borderColor: 'gray',
    borderWidth: 1,
  },
});
